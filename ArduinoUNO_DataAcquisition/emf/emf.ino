#define PIN_ANTENNA1 A0
#define PIN_ANTENNA2 A1
#define PIN_ANTENNA3 A2
#define PIN_ANTENNA4 A3
#define PIN_ANTENNA5 A4
#define PIN_ANTENNA6 A5

// How many samples to take for the average. 
// Higher = Smoother numbers, but slower reaction time.
// 50 is a good balance for EMF.
#define SAMPLES 5000 

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
  // Read once and discard to switch the ADC capacitor to this pin
  analogRead(pin); 
  delay(2); // Short wait for voltage to settle

  // 2. TAKE SAMPLES
  for(int i = 0; i < SAMPLES; i++)
  {
    sum += analogRead(pin);
    // No delay needed inside this loop; we want to catch the wave peaks quickly
  }

  // 3. CALCULATE AVERAGE
  return (int)(sum / SAMPLES);
}

void loop()
{
  Serial.println("--- Average Readings ---");
  
  Serial.print("Antenna 1: ");
  Serial.println(getAverageReading(PIN_ANTENNA1));
  
  Serial.print("Antenna 2: ");
  Serial.println(getAverageReading(PIN_ANTENNA2));

  Serial.print("Antenna 3: ");
  Serial.println(getAverageReading(PIN_ANTENNA3));

  Serial.print("Antenna 4: ");
  Serial.println(getAverageReading(PIN_ANTENNA4));

  Serial.print("Antenna 5: ");
  Serial.println(getAverageReading(PIN_ANTENNA5));
  
  Serial.print("Antenna 6: ");
  Serial.println(getAverageReading(PIN_ANTENNA6));

  // The main delay you requested
  delay(500); 
}