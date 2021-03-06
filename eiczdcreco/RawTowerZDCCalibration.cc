#include "RawTowerZDCCalibration.h"

#include <eiczdcbase/RawTowerZDC.h>
#include <eiczdcbase/RawTowerZDCContainer.h>
#include <eiczdcbase/RawTowerZDCDefs.h>
#include <eiczdcbase/RawTowerZDCGeom.h>
#include <eiczdcbase/RawTowerZDCGeomContainer.h>
#include <eiczdcbase/RawTowerZDCv1.h>

#include <phparameter/PHParameters.h>

#include <fun4all/Fun4AllReturnCodes.h>
#include <fun4all/SubsysReco.h>

#include <phool/PHCompositeNode.h>
#include <phool/PHIODataNode.h>
#include <phool/PHNode.h>
#include <phool/PHNodeIterator.h>
#include <phool/PHObject.h>
#include <phool/getClass.h>

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

using namespace std;

RawTowerZDCCalibration::RawTowerZDCCalibration(const std::string &name)
  : SubsysReco(name)
  , _calib_algorithm(kNo_calibration)
  ,  //
  _calib_towers(nullptr)
  , _raw_towers(nullptr)
  ,  //
  rawtowergeom(nullptr)
  ,  //
  detector("NONE")
  ,  //
  _calib_tower_node_prefix("CALIB")
  , _raw_tower_node_prefix("RAW")
  ,  //
  //! pedestal in unit of ADC
  _pedestal_ADC(NAN)
  ,
  //! default to fixed pedestal
  _pedestal_file(false)
  ,
  //! calibration constant in unit of GeV per ADC
  _calib_const_GeV_ADC(NAN)
  ,  //
  //! default to fixed GeV per ADC
  _GeV_ADC_file(false)
  , _tower_type(-1)
  , _tower_calib_params(name)
{
}

int RawTowerZDCCalibration::InitRun(PHCompositeNode *topNode)
{
  PHNodeIterator iter(topNode);

  // Looking for the DST node
  PHCompositeNode *dstNode;
  dstNode = dynamic_cast<PHCompositeNode *>(iter.findFirst("PHCompositeNode",
                                                           "DST"));
  if (!dstNode)
  {
    std::cout << Name() << "::" << detector << "::" << __PRETTY_FUNCTION__
              << "DST Node missing, doing nothing." << std::endl;
    exit(1);
  }

  try
  {
    CreateNodes(topNode);
  }
  catch (std::exception &e)
  {
    std::cout << e.what() << std::endl;
    return Fun4AllReturnCodes::ABORTRUN;
  }
  return Fun4AllReturnCodes::EVENT_OK;
}

int RawTowerZDCCalibration::process_event(PHCompositeNode */*topNode*/)
{
  if (Verbosity())
  {
    std::cout << Name() << "::" << detector << "::" << __PRETTY_FUNCTION__
              << "Process event entered" << std::endl;
  }

  RawTowerZDCContainer::ConstRange begin_end = _raw_towers->getTowers();
  RawTowerZDCContainer::ConstIterator rtiter;
  for (rtiter = begin_end.first; rtiter != begin_end.second; ++rtiter)
  {
    const RawTowerZDCDefs::keytype key = rtiter->first;
    
    const RawTowerZDC *raw_tower = rtiter->second;
    assert(raw_tower);

    RawTowerZDCGeom *raw_tower_geom = rawtowergeom->get_tower_geometry(
        raw_tower->get_id());
    assert(raw_tower_geom);

    if (_tower_type >= 0)
    {
      // Skip towers that don't match the type we are supposed to calibrate
      if (_tower_type != raw_tower_geom->get_tower_type())
      {
        continue;
      }
    }

    if (_calib_algorithm == kNo_calibration)
    {
      _calib_towers->AddTower(key, new RawTowerZDCv1(*raw_tower));
    }
    else if (_calib_algorithm == kSimple_linear_calibration)
    {
      const double raw_energy = raw_tower->get_energy();
      const double calib_energy = (raw_energy - _pedestal_ADC) * _calib_const_GeV_ADC;

      RawTowerZDC *calib_tower = new RawTowerZDCv1(*raw_tower);
      calib_tower->set_energy(calib_energy);
      _calib_towers->AddTower(key, calib_tower);
    }
    else if (_calib_algorithm == kTower_by_tower_calibration)
    {
      //      RawTowerZDCDefs::CalorimeterId caloid = RawTowerZDCDefs::decode_caloid(key);
      const int eta = raw_tower->get_bineta();
      const int phi = raw_tower->get_binphi();
      const int twr   = raw_tower->get_binl();

      double tower_by_tower_calib = 1.;
      const string calib_const_name("calib_const_eta" + to_string(eta) + "_phi" + to_string(phi) + "_twr" + to_string(twr));

      tower_by_tower_calib = _tower_calib_params.get_double_param(calib_const_name);

      if (_pedestal_file == true)
      {
	const string pedestal_name("PedCentral_ADC_eta" + to_string(eta) + "_phi" + to_string(phi)+ "_twr" + to_string(twr));
	_pedestal_ADC = _tower_calib_params.get_double_param(pedestal_name);
      }

      if (_GeV_ADC_file == true)
      {
	const string GeVperADCname("GeVperADC_eta" + to_string(eta) + "_phi" + to_string(phi)+ "_twr" + to_string(twr));
	_calib_const_GeV_ADC = _tower_calib_params.get_double_param(GeVperADCname);
      }
       
      const double raw_energy = raw_tower->get_energy();
      const double calib_energy = (raw_energy - _pedestal_ADC) * _calib_const_GeV_ADC * tower_by_tower_calib;


      RawTowerZDC *calib_tower = new RawTowerZDCv1(*raw_tower);
      calib_tower->set_energy(calib_energy);
      _calib_towers->AddTower(key, calib_tower);
    }
    else
    {
      std::cout << Name() << "::" << detector << "::" << __PRETTY_FUNCTION__
                << " invalid calibration algorithm #" << _calib_algorithm
                << std::endl;

      return Fun4AllReturnCodes::ABORTRUN;
    }
  }  //  for (rtiter = begin_end.first; rtiter != begin_end.second; ++rtiter)

  if (Verbosity())
  {
    std::cout << Name() << "::" << detector << "::" << __PRETTY_FUNCTION__
              << "input sum energy = " << _raw_towers->getTotalEdep()
              << ", output sum digitalized value = "
              << _calib_towers->getTotalEdep() << std::endl;
  }
  return Fun4AllReturnCodes::EVENT_OK;
}

int RawTowerZDCCalibration::End(PHCompositeNode */*topNode*/)
{
  return Fun4AllReturnCodes::EVENT_OK;
}

void RawTowerZDCCalibration::CreateNodes(PHCompositeNode *topNode)
{
  PHNodeIterator iter(topNode);
  PHCompositeNode *runNode = static_cast<PHCompositeNode *>(iter.findFirst(
      "PHCompositeNode", "RUN"));
  if (!runNode)
  {
    std::cerr << Name() << "::" << detector << "::" << __PRETTY_FUNCTION__
              << "Run Node missing, doing nothing." << std::endl;
    throw std::runtime_error(
        "Failed to find Run node in RawTowerZDCCalibration::CreateNodes");
  }

  TowerGeomNodeName = "TOWERGEOM_" + detector;
  rawtowergeom = findNode::getClass<RawTowerZDCGeomContainer>(topNode,
                                                           TowerGeomNodeName);
  if (!rawtowergeom)
  {
    std::cerr << Name() << "::" << detector << "::" << __PRETTY_FUNCTION__
              << " " << TowerGeomNodeName << " Node missing, doing bail out!"
              << std::endl;
    throw std::runtime_error(
        "Failed to find " + TowerGeomNodeName + " node in RawTowerZDCCalibration::CreateNodes");
  }

  if (Verbosity() >= 1)
  {
    rawtowergeom->identify();
  }

  PHCompositeNode *dstNode = dynamic_cast<PHCompositeNode *>(iter.findFirst(
      "PHCompositeNode", "DST"));
  if (!dstNode)
  {
    std::cerr << Name() << "::" << detector << "::" << __PRETTY_FUNCTION__
              << "DST Node missing, doing nothing." << std::endl;
    throw std::runtime_error(
        "Failed to find DST node in RawTowerZDCCalibration::CreateNodes");
  }

  RawTowerNodeName = "TOWER_" + _raw_tower_node_prefix + "_" + detector;
  _raw_towers = findNode::getClass<RawTowerZDCContainer>(dstNode,
                                                      RawTowerNodeName);
  if (!_raw_towers)
  {
    std::cerr << Name() << "::" << detector << "::" << __PRETTY_FUNCTION__
              << " " << RawTowerNodeName << " Node missing, doing bail out!"
              << std::endl;
    throw std::runtime_error(
        "Failed to find " + RawTowerNodeName + " node in RawTowerZDCCalibration::CreateNodes");
  }

  // Create the tower nodes on the tree
  PHNodeIterator dstiter(dstNode);
  PHCompositeNode *DetNode = dynamic_cast<PHCompositeNode *>(dstiter.findFirst(
      "PHCompositeNode", detector));
  if (!DetNode)
  {
    DetNode = new PHCompositeNode(detector);
    dstNode->addNode(DetNode);
  }

  // Be careful as a previous calibrator may have been registered for this detector
  CaliTowerNodeName = "TOWER_" + _calib_tower_node_prefix + "_" + detector;
  _calib_towers = findNode::getClass<RawTowerZDCContainer>(DetNode,
                                                        CaliTowerNodeName);
  if (!_calib_towers)
  {
    _calib_towers = new RawTowerZDCContainer(_raw_towers->getCalorimeterID());
    PHIODataNode<PHObject> *towerNode = new PHIODataNode<PHObject>(_calib_towers, CaliTowerNodeName, "PHObject");
    DetNode->addNode(towerNode);
  }
  return;
}
