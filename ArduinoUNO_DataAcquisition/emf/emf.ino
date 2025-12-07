#define PIN_ANTENNA1 A0
#define PIN_ANTENNA2 A1
#define PIN_ANTENNA3 A2
#define PIN_ANTENNA4 A3
#define PIN_ANTENNA5 A4
#define PIN_ANTENNA6 A5

// How many samples to take for the average.
#define SAMPLES 500

// The threshold to determine if signal is HIGH (1) or LOW (0)
#define THRESHOLD 120

//Delay intre citiri, trb sa fie >=2
#define DELAY_BETWEEN_READ 2

void setup()
{
  Serial.begin(9600);
  pinMode(PIN_ANTENNA1, INPUT);
  pinMode(PIN_ANTENNA2, INPUT);
  pinMode(PIN_ANTENNA3, INPUT);
  pinMode(PIN_ANTENNA4, INPUT);
  pinMode(PIN_ANTENNA5, INPUT);
  pinMode(PIN_ANTENNA6, INPUT);
}

// Function to read a pin multiple times and return the average
int getAverageReading(int pin)
{
  long sum = 0;

  // 1. CLEAR THE GHOST
  analogRead(pin); 
  delay(DELAY_BETWEEN_READ); 

  // 2. TAKE SAMPLES
  for(int i = 0; i < SAMPLES; i++)
  {
    sum += analogRead(pin);
  }

  // 3. CALCULATE AVERAGE
  return (int)(sum / SAMPLES);
}

// Logic to convert the raw reading into 1 or 0
int getBooleanState(int reading)
{
  if (reading > THRESHOLD) {
    return 1;
  } else {
    return 0;
  }
}

void sendData()
{
  // We collect the data first to keep the output clean
  int val1 = getAverageReading(PIN_ANTENNA1);
  int val2 = getAverageReading(PIN_ANTENNA2);
  int val3 = getAverageReading(PIN_ANTENNA3);
  int val4 = getAverageReading(PIN_ANTENNA4);
  int val5 = getAverageReading(PIN_ANTENNA5);
  int val6 = getAverageReading(PIN_ANTENNA6);

  // Print in the format: "1,1 2,1 3,0 4,0 5,1 6,0"
  
  Serial.print("1,");
  Serial.print(getBooleanState(val1));
  Serial.print(" "); // Space separator

  Serial.print("2,");
  Serial.print(getBooleanState(val2));
  Serial.print(" ");

  Serial.print("3,");
  Serial.print(getBooleanState(val3));
  Serial.print(" ");

  Serial.print("4,");
  Serial.print(getBooleanState(val4));
  Serial.print(" ");

  Serial.print("5,");
  Serial.print(getBooleanState(val5));
  Serial.print(" ");

  Serial.print("6,");
  Serial.print(getBooleanState(val6));
  
  // End the line so the receiver knows the packet is finished
  Serial.println(); 
}

void loop()
{
  sendData();
  
  // Wait before sending the next packet
  delay(500); 
}