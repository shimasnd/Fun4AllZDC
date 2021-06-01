#ifndef ZDCNTUPLE_H
#define ZDCNTUPLE_H

#include <fun4all/SubsysReco.h>

#include <map>
#include <set>
#include <string>
#include <vector>

// Forward declerations
class Fun4AllHistoManager;
class PHCompositeNode;
class TFile;
class TNtuple;

class ZDCNtuple : public SubsysReco
{
 public:
  //! constructor
  ZDCNtuple(const std::string &name = "ZDCNtuple", const std::string &filename = "ZDCNtuple.root");

  //! destructor
  ~ZDCNtuple() override;

  //! full initialization
  int Init(PHCompositeNode *) override;

  //! event processing method
  int process_event(PHCompositeNode *) override;

  //! end of run method
  int End(PHCompositeNode *) override;

  void AddNode(const std::string &name, const int detid = 0);

 protected:
  int nblocks;
  Fun4AllHistoManager *hm;
  std::string _filename;
  std::set<std::string> _node_postfix;
  std::map<std::string, int> _detid;
  TNtuple *ntup;
  TFile *outfile;
};

#endif
