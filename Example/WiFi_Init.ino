
void setup() 
{
  // initialize serial communication at 9600 bits per second:
  Serial.begin(9600);
  delay(100);
  
  Serial.println("/store /SSID.txt FlinBit_111111");
  delay(100);

  Serial.println("/store /password.txt secure987");
  delay(100);
}

// the loop routine runs over and over again forever:
void loop() 
{
  Serial.println("WiFi settings updated");
  delay (1000);
}

