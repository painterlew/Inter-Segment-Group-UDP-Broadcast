#include "UdpRelay.h"
#include <iostream>   // cerr
#include <string>

using namespace std;


bool readIp_port(string input, string& groupIp, int& port);

int main( int argc, char *argv[] ) {
  // verify the argument.
  if ( argc != 2 ) {
    cerr << "usage: bcast groupIp:groupPort" << endl;
    return -1;
  }


  UdpRelay udprelay(argv[1]);
  

  return 0;
}


