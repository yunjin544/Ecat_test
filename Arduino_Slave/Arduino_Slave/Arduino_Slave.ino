#include "EasyCAT.h"                // EasyCAT library to interface the LAN9252
#include <SPI.h>                    // SPI library


EasyCAT EASYCAT;


//---- pins declaration ------------------------------------------------------------------------------

const int BitOut0 = A2;             // digital output bit 0

//---- global variables ---------------------------------------------------------------------------


//---- setup ---------------------------------------------------------------------------------------

void setup()
{
  Serial.begin(115200);

  Serial.print ("\n DRCL EtherCAT TEST Plaform with EasyCAT \n");

  pinMode(BitOut0, OUTPUT);



  if (EASYCAT.Init() == true)
  {
    Serial.print ("initialized");
  }

  else
  {
    Serial.print ("initialization failed");

    pinMode(13, OUTPUT);

    while (1)
    {
      digitalWrite (13, LOW);
      delay(500);
      digitalWrite (13, HIGH);
      delay(500);
    }
  }

}


//---- main loop ----------------------------------------------------------------------------------------

void loop()                                             // In the main loop we must call ciclically the
{ // EasyCAT task and our application
  //
  EASYCAT.MainTask();                                   // execute the EasyCAT task

  Application();                                        // user applications
}



//---- user application ------------------------------------------------------------------------------

void Application ()

{

  if (EASYCAT.BufferOut.Byte[0] == 1)
  {
    digitalWrite (BitOut0, HIGH);
    Serial.print("1\n");
  }

  else {
    digitalWrite (BitOut0, LOW);
    Serial.print("0\n");
  }


}
