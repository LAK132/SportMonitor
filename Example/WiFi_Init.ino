/*
 * Program to initalise the WiFi SSID (Access point name) and password.
 * 
 * The SSID should be a maximum of 63 characters and contain no white space.
 * The password needs to be a minimum of 8 characters and a maximum of 80 characters.
 *
 */

void setup() 
{
  // initialize serial communication at 9600 bits per second:
  Serial.begin(9600);
  delay(100);

  // Please change 'MyAccessPointName' to a unique name to avoid conflicting with other FlinBITs   
  Serial.println("/store /SSID.txt MyAccessPointName");
  delay(100);

  // Optionally, you can change the default password 'secure123'
  Serial.println("/store /password.txt secure123");
  delay(100);
}

// the loop routine runs over and over again forever:
void loop() 
{
  Serial.println("WiFi settings updated");
  delay (1000);
}
