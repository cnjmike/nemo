// ============================================================================
// Copyright Jean-Charles LAMBERT - 2004                                       
// e-mail:   Jean-Charles.Lambert@oamp.fr                                      
// address:  Dynamique des galaxies                                            
//           Laboratoire d'Astrophysique de Marseille                          
//           2, place Le Verrier                                               
//           13248 Marseille Cedex 4, France                                   
//           CNRS U.M.R 6110                                                   
// ============================================================================
// See the complete license in LICENSE and/or "http://www.cecill.info".        
// ============================================================================
//                                                                             
// definition of the NetworkData Class                                         
//                                                                             
//                                                                             
// ============================================================================

#ifndef NETWORK_DATA_H
#define NETWORK_DATA_H

#include <qobject.h>

#include "particles_range.h"
#include "virtual_data.h"
#include "SocketClient.h"
#include "MessageBuffer.h"
#include <string>

class NetworkData : public VirtualData
{    
  Q_OBJECT
 public: 
  NetworkData(const char * hostname);
  ~NetworkData();
  int loadPos(ParticlesRangeVector * prv);
  int getNbody();
  float * getPos() { return pos; };
  float  getTime();
  float * getCooMax() {return coo_max;};
  int * getCooIndexMax() {return i_max;}; 
  const char * getDataName() { return data_name.c_str(); };
  const char * getDataType() { return "Server: "; };
  bool isConnected() { return is_connected ; };
  void uploadGlData(ParticlesRangeVector *);
  void setSelectedRange(const QString s) { qselect_part=s; };
  QString endOfDataMessage();
 signals:
  void loadedData(const int *, const float *, const ParticlesRangeVector*);

 private:
  const  char * select_part, * select_time;
  QString qselect_part;
  char  * sel2;
  std::string data_name;

  bool is_open;   // TRUE if file has been open
  bool is_parsed; // TRUE if particle string has been parsed
  bool is_new_data_loaded; 
    
  SocketClient * client;
  int sock_fd;
  bool is_connected;
  MessageBuffer * clientMB;
  
  //ParticlesRangeVector prv; // store Particles Range

  // method
  int close(); // close snapshot
};
#endif 
