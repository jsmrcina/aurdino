#include <pitches.h>
#include <LiquidCrystal.h>

// Pin definitions
#define LED_PIN 2
#define MORSE_PIN 3
#define RESET_PIN 4
#define BUZZER_PIN 5

// Info on morse button state
int currentMorseButtonState;
unsigned long lastStateChangeMillis;

// Info on reset button state
int currentResetButtonState;

// Symbol definitions
#define QUANTUM 10 // Shortest period we use for multiplier
#define DOT_LENGTH 3 * QUANTUM // Length of morse dot
#define DASH_LENGTH 15 * QUANTUM // Length of morse dash
#define END_SYMBOL 25 * QUANTUM // Length of end of single letter
#define END_WORD 250 * QUANTUM // Length of end of word (insert space)
#define PITCH 5 * QUANTUM // Length of emitted pitch for speaker

// Symbol history
#define HISTORY_LENGTH 5
int historyIndex;
int symbolHistory[HISTORY_LENGTH];
bool wordStarted;

//
// Symbols
// This table contains a base-3 encoded set of symbols for morse code. We only use 26 code points
// but need 78 elements to encompass the largest base-3 encoded point ('Y' is base-3 encoded to '77')
// This could be optimized (e.g. a case table) but this works nicely.
//
#define SYMBOL_SIZE 78
char symbols[SYMBOL_SIZE];

// LCD Output
LiquidCrystal lcd(7, 8, 9, 10, 11, 12);
#define OUTPUT_SIZE 16
char output[OUTPUT_SIZE];
int outputIndex;

/*
  Sets up decoding table for morse code characters as a base-3 encoded number (dots / dashes / silence)
  to a letter map.

  For example, A is represented as [0] = dot, [1] = dash, [2-5] = silence. We invert the array when
  transforming to a base-3 number so that index 0 is the LSB.

  silence = 0
  dot = 1
  dash = 2

  Thus:
    'A' (dot, dash) is encoded as ((3 ^ 0) * 1) + ((3 ^ 1) * 2) = 7
    'B' (dash, dot, dot, dot) is encoded as ((3 ^ 0) * 2) + ((3 ^ 1) * 1) + ((3 ^ 2) * 1) + ((3 ^ 3) * 1) = 41
    and so on.
*/
void initSymbols()
{
  for(int x = 0; x < SYMBOL_SIZE; x++)
  {
    symbols[x] = 0;
  }

  symbols[7] = 'A'; symbols[41] = 'B'; symbols[50] = 'C'; symbols[14] = 'D';
  symbols[1] = 'E'; symbols[49] = 'F'; symbols[17] = 'G'; symbols[40] = 'H';
  symbols[4] = 'I'; symbols[79] = 'J'; symbols[23] = 'K'; symbols[43] = 'L';
  symbols[8] = 'M'; symbols[5] = 'N'; symbols[26] = 'O'; symbols[52] = 'P';
  symbols[71] = 'Q'; symbols[16] = 'R'; symbols[13] = 'S'; symbols[2] = 'T';
  symbols[22] = 'U'; symbols[67] = 'V'; symbols[25] = 'W'; symbols[68] = 'X';
  symbols[77] = 'Y'; symbols[44] = 'Z';
}

/*
  Sets up the Arduino board
*/
void setup() 
{
  // Set baud rate
  Serial.begin(115200);

  // Assign PIN mods
  pinMode(LED_PIN, OUTPUT);
  pinMode(MORSE_PIN, INPUT_PULLUP);
  pinMode(RESET_PIN, INPUT_PULLUP);

  // Set up symbol decoding table
  initSymbols();

  // Clear history array
  wordStarted = false;
  clearHistory();

  // Clear LCD output
  clearOutput();

  // Default button states and initial time
  currentMorseButtonState = HIGH;
  currentResetButtonState = HIGH;
  lastStateChangeMillis = millis();

  // Initialize LCD
  lcd.begin(OUTPUT_SIZE, 2);
}

/*
  Clears history for a single character (dot/dash/silence encoding array)
*/
void clearHistory()
{
  historyIndex = 0;
  for(int x = 0; x < HISTORY_LENGTH; x++)
  {
    symbolHistory[x] = 0;
  }
}

/*
  Clears output array for LCD
*/
void clearOutput()
{
  outputIndex = 0;
  for(int x = 0; x < OUTPUT_SIZE; x++)
  {
    output[x] = '\0';
  }
}

/*
  Updates the LCD with a single new character
*/
void updateLcd(char newChar)
{
  // Put the character in our output array and wrap
  output[outputIndex] = newChar;
  outputIndex = (outputIndex + 1) % OUTPUT_SIZE;

  // Print the whole array to LCD
  lcd.setCursor(0, 0);
  lcd.print(output);
}

/*
  Clears the whole first row of the LCD to blank
*/
void clearLcd()
{
  clearOutput();
  lcd.setCursor(0, 0);
  lcd.print("                ");
}

/*
  Called when a symbol end timeout is hit. Ends a single morse symbol
*/
void endSymbol()
{
  int index = 0;
  int multiplier = 1; // 3 ^ 0
  for(int x = 0; x < HISTORY_LENGTH; x++)
  {
    // We're in a 3-based system here (0 is not set, 1 is short, 2 is long)
    index += symbolHistory[x] * multiplier;
    multiplier *= 3;
  }

  if(index != 0 && index < SYMBOL_SIZE && symbols[index] != 0)
  {
    updateLcd(symbols[index]);
  }
  else if(index != 0)
  {
    updateLcd('!');
  }
}

/*
  Hit when a word end timeout is hit. Ends a word with a space.
*/
void endWord()
{
  if(wordStarted)
  {
    updateLcd(' ');
  }
}

/*
  Adds a dash/dot to the history table (later used for evaluating a single symbol or letter).

Arguments:
  changed - True if the button state has changed since last invocation
  buttonState - The current button state (LOW / HIGH)
  length - The length of time that the button has been in the buttonState
*/
int evaluateSymbolOnMorseButtonChange(bool changed, int buttonState, unsigned long length)
{
  if(changed)
  {
    if(buttonState == LOW && length > DASH_LENGTH)
    {
      // Long (dash)
      symbolHistory[historyIndex] = 2;
      historyIndex = (historyIndex + 1) % HISTORY_LENGTH;
      wordStarted = true;
    }
    else if(buttonState == LOW && length > DOT_LENGTH)
    {
      // Short (dot)
      symbolHistory[historyIndex] = 1;
      historyIndex = (historyIndex + 1) % HISTORY_LENGTH;
      wordStarted = true;
    }
  }
  else
  {
    //
    // If we have been in the high state for longer than END_WORD, and there has been no change
    // then it is time to end the symbol and the word.
    //
    if(buttonState == HIGH && length > END_WORD)
    {
      endSymbol();
      endWord();
      clearHistory();
      wordStarted = false;
    }
    //
    // If we have been in the high state for longer than END_SYMBOL, and there has been no change
    // then it is time to end the symbol.
    //
    else if(buttonState == HIGH && length > END_SYMBOL)
    {
      endSymbol();
      clearHistory();
    }
  }
}

/*
  Called on each update loop if the reset button is not being held.

Arguments:
  newMorseButtonState - The new state of the morse code button (LOW / HIGH)
*/
void updateSymbols(int newMorseButtonState)
{
      // Check how long it's been
    unsigned long currentTimeMillis = millis();
    unsigned long stateMillis = currentTimeMillis - lastStateChangeMillis;

    if(newMorseButtonState != currentMorseButtonState)
    {
      evaluateSymbolOnMorseButtonChange(true, currentMorseButtonState, stateMillis);

      // Update current state
      currentMorseButtonState = newMorseButtonState;
      lastStateChangeMillis = currentTimeMillis;
    }
    else if(newMorseButtonState == HIGH)
    {
      evaluateSymbolOnMorseButtonChange(false, currentMorseButtonState, stateMillis);
    }
}

/*
  Lights the LED based on the morse code button state.

Arguments:
  newMorseButtonState - The new state of the morse code button (LOW / HIGH)
*/
void updateLed(int newMorseButtonState)
{
  if (newMorseButtonState == HIGH)
  {
    digitalWrite(LED_PIN, LOW);
  }
  else
  {
    digitalWrite(LED_PIN, HIGH);
  }
}

/*
  Buzzes the active buzzer based on the morse code button state.

Arguments:
  newMorseButtonState - The new state of the morse code button (LOW / HIGH)
*/

void updateBuzzer(int newMorseButtonState)
{
  if(newMorseButtonState == LOW)
  {
      tone(BUZZER_PIN, NOTE_C6, PITCH);
  }
}

/*
  Resets all internal data (used when reset button is pressed).
*/
void resetAll()
{
  clearHistory();
  clearLcd();
  updateLed(HIGH);
  updateBuzzer(HIGH);
  wordStarted = false;
}

/*
  Main Arduino loop
*/
void loop() 
{
  int newResetButtonState = digitalRead(RESET_PIN);

  // If this is a reset, ignore everything else
  if(currentResetButtonState == HIGH && newResetButtonState == LOW)
  {
    currentResetButtonState = newResetButtonState;
    resetAll();
  }
  else if(currentResetButtonState == LOW && newResetButtonState == LOW)
  {
    // Do nothing while reset is held down
  }
  else
  {
    // Reset the reset button state
    currentResetButtonState = HIGH;

    int newMorseButtonState = digitalRead(MORSE_PIN);
    updateSymbols(newMorseButtonState);
    updateLed(newMorseButtonState);
    updateBuzzer(newMorseButtonState);
  }
}
