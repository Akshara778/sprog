#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

// Constants
#define STEP_SIZE 0.0001
#define MAX_EXPR_LENGTH 50
#define MAX_STACK_SIZE 100

// Stack Structures
typedef struct {
    char arr[MAX_STACK_SIZE][3];
    int top;
} Stack;

typedef struct {
    float arr[MAX_STACK_SIZE];
    int top;
} NumStack;

// Function Declarations
double reduce_angle(double rad);
double deg2rad(double deg);
double tangent_rk4(double radians, double h);
double compute_ln(double x);
void decimal_to_fraction(double decimal, int *numerator, int *denominator);
void infixToPostfix(const char* infix, char* postfix);
float evaluatePostfix(const char* postfix);

// Stack Functions
void initStack(Stack *s) { s->top = -1; }
void initNumStack(NumStack *s) { s->top = -1; }
int isEmpty(Stack *s) { return s->top == -1; }
int isEmptyNum(NumStack *s) { return s->top == -1; }

const char* peek(Stack *s) { return isEmpty(s) ? "" : s->arr[s->top]; }
void push(Stack *s, const char* val) { 
    if (s->top < MAX_STACK_SIZE - 1) {
        strcpy(s->arr[++s->top], val);
    }
}
void pop(Stack *s) { if (!isEmpty(s)) s->top--; }

void pushNum(NumStack *s, float val) { 
    if (s->top < MAX_STACK_SIZE - 1) s->arr[++s->top] = val; 
}
float popNum(NumStack *s) { 
    return isEmptyNum(s) ? 0 : s->arr[s->top--]; 
}

// Utility Functions
char* itoa(int value, char* str, int base) {
    char* ptr = str, *ptr1 = str, tmp_char;
    int tmp_value;
    
    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "0123456789abcdef"[tmp_value - value * base];
    } while (value);
    
    *ptr-- = '\0';
    
    // Reverse string
    while(ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
    
    return str;
}

// LCD Pin Definitions for Arduino UNO
#define LCD_RS_PORT PORTD
#define LCD_RS_PIN 2
#define LCD_EN_PORT PORTD
#define LCD_EN_PIN 3
#define LCD_D4_PORT PORTD
#define LCD_D4_PIN 4
#define LCD_D5_PORT PORTD
#define LCD_D5_PIN 5
#define LCD_D6_PORT PORTD
#define LCD_D6_PIN 6
#define LCD_D7_PORT PORTD
#define LCD_D7_PIN 7

// LCD Commands
#define LCD_CLEARDISPLAY 0x01
#define LCD_RETURNHOME 0x02
#define LCD_ENTRYMODESET 0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_CURSORSHIFT 0x10
#define LCD_FUNCTIONSET 0x20
#define LCD_SETCGRAMADDR 0x40
#define LCD_SETDDRAMADDR 0x80

// LCD Flags
#define LCD_ENTRYRIGHT 0x00
#define LCD_ENTRYLEFT 0x02
#define LCD_ENTRYSHIFTINCREMENT 0x01
#define LCD_ENTRYSHIFTDECREMENT 0x00
#define LCD_DISPLAYON 0x04
#define LCD_DISPLAYOFF 0x00
#define LCD_CURSORON 0x02
#define LCD_CURSOROFF 0x00
#define LCD_BLINKON 0x01
#define LCD_BLINKOFF 0x00
#define LCD_DISPLAYMOVE 0x08
#define LCD_CURSORMOVE 0x00
#define LCD_MOVERIGHT 0x04
#define LCD_MOVELEFT 0x00
#define LCD_8BITMODE 0x10
#define LCD_4BITMODE 0x00
#define LCD_2LINE 0x08
#define LCD_1LINE 0x00
#define LCD_5x10DOTS 0x04
#define LCD_5x8DOTS 0x00

// ADC Pin Definitions
#define ADC_PORT PORTC
#define ADC_DDR DDRC
#define NUM_PAD_ADC 0    // A0 for number pad only
#define FUNC1_ADC 1      // A1 for sin, cos, tan
#define FUNC2_ADC 2      // A2 for ln, log10

// Button Pin Definitions
#define BUTTON_PORT PORTB
#define BUTTON_DDR DDRB
#define ENTER_BUTTON PB0    // Digital pin 8
#define PAREN_BUTTON PB1    // Digital pin 9
#define OPERATOR1_BUTTON PB2  // Digital pin 10
#define OPERATOR2_BUTTON PB3  // Digital pin 11
#define OPERATOR3_BUTTON PB4  // Digital pin 12
#define OPERATOR4_BUTTON PB5  // Digital pin 13

// Function button pins (using analog pins as digital)
#define FUNCTION_PORT PORTC
#define FUNCTION_DDR DDRC
#define SIN_BUTTON PC1     // A1
#define COS_BUTTON PC2     // A2
#define TAN_BUTTON PC3     // A3
#define LN_BUTTON PC4      // A4
#define LOG10_BUTTON PC5   // A5

// Global Variables
char userInput[50];
int inputPos = 0;
int num_brackets = 0;
float temp = 0;
unsigned long buttonPressTime = 0;
unsigned long currentTime = 0;
#define BUTTON_HOLD_TIME 500  // 500ms hold time for right parenthesis

// Function state variables
uint8_t func1_state = 0;  // 0: no function, 1: sin, 2: cos, 3: tan
uint8_t func2_state = 0;  // 0: no function, 1: ln, 2: log10
uint8_t last_func1_press = 0;
uint8_t last_func2_press = 0;

// LCD Functions
void lcd_write_nibble(uint8_t nibble) {
    // Clear all data pins first
    LCD_D4_PORT &= ~(1 << LCD_D4_PIN);
    LCD_D5_PORT &= ~(1 << LCD_D5_PIN);
    LCD_D6_PORT &= ~(1 << LCD_D6_PIN);
    LCD_D7_PORT &= ~(1 << LCD_D7_PIN);
    
    // Set data pins according to nibble
    if(nibble & 0x01) LCD_D4_PORT |= (1 << LCD_D4_PIN);
    if(nibble & 0x02) LCD_D5_PORT |= (1 << LCD_D5_PIN);
    if(nibble & 0x04) LCD_D6_PORT |= (1 << LCD_D6_PIN);
    if(nibble & 0x08) LCD_D7_PORT |= (1 << LCD_D7_PIN);
    
    // Pulse enable pin
    LCD_EN_PORT |= (1 << LCD_EN_PIN);
    _delay_us(10);  // Increased pulse width
    LCD_EN_PORT &= ~(1 << LCD_EN_PIN);
    _delay_us(10);  // Increased delay after pulse
}

void lcd_write_byte(uint8_t byte) {
    lcd_write_nibble(byte >> 4);    // High nibble
    _delay_us(10);  // Increased delay between nibbles
    lcd_write_nibble(byte & 0x0F);  // Low nibble
}

void lcd_command(uint8_t cmd) {
    LCD_RS_PORT &= ~(1 << LCD_RS_PIN);  // Command mode
    lcd_write_byte(cmd);
    _delay_ms(10);  // Increased delay for commands
}

void lcd_data(uint8_t data) {
    LCD_RS_PORT |= (1 << LCD_RS_PIN);   // Data mode
    lcd_write_byte(data);
    _delay_us(200);  // Increased delay for data
}

void lcd_init(void) {
    // Configure LCD pins as outputs
    DDRD |= (1 << LCD_RS_PIN) | (1 << LCD_EN_PIN) | 
            (1 << LCD_D4_PIN) | (1 << LCD_D5_PIN) | 
            (1 << LCD_D6_PIN) | (1 << LCD_D7_PIN);
    
    // Initialize LCD in 4-bit mode
    _delay_ms(100);  // Wait for power-up
    
    // Reset LCD to 4-bit mode
    lcd_write_nibble(0x03);  // Set 4-bit mode
    _delay_ms(10);
    lcd_write_nibble(0x03);
    _delay_ms(10);
    lcd_write_nibble(0x03);
    _delay_ms(10);
    lcd_write_nibble(0x02);  // Set 4-bit mode
    _delay_ms(10);
    
    // Function set command
    lcd_command(LCD_FUNCTIONSET | LCD_4BITMODE | LCD_2LINE | LCD_5x8DOTS);
    _delay_ms(10);
    
    // Display control: display on, cursor off, blink off
    lcd_command(LCD_DISPLAYCONTROL | LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF);
    _delay_ms(10);
    
    // Entry mode set: increment cursor, no display shift
    lcd_command(LCD_ENTRYMODESET | LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT);
    _delay_ms(10);
    
    // Clear display
    lcd_command(LCD_CLEARDISPLAY);
    _delay_ms(20);
    
    // Return home
    lcd_command(LCD_RETURNHOME);
    _delay_ms(20);
}

void lcd_print(const char* str) {
    while(*str) {
        lcd_data(*str++);
        _delay_us(200);  // Increased delay between characters
    }
}

void lcd_clear(void) {
    lcd_command(LCD_CLEARDISPLAY);
    _delay_ms(20);  // Increased delay after clear
}

void lcd_set_cursor(uint8_t col, uint8_t row) {
    int row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };
    lcd_command(LCD_SETDDRAMADDR | (col + row_offsets[row]));
    _delay_ms(10);  // Increased delay after cursor movement
}

// ADC Functions
void adc_init(void) {
    // Configure ADC pins as inputs
    DDRC &= ~((1 << 0) | (1 << 1) | (1 << 2));  // Set PC0, PC1, PC2 as inputs
    PORTC &= ~((1 << 0) | (1 << 1) | (1 << 2)); // Disable pull-ups
    
    // Configure ADC with more stable settings
    ADMUX = (1 << REFS0);  // Reference voltage from AVCC
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);  // Enable ADC, prescaler 128
    
    // Do multiple initial conversions to stabilize ADC
    for(uint8_t i = 0; i < 10; i++) {
        ADCSRA |= (1 << ADSC);
        while(ADCSRA & (1 << ADSC));
        _delay_ms(2);
    }
}

uint16_t adc_read(uint8_t channel) {
    static uint16_t last_value = 0;
    static uint8_t stable_count = 0;
    static uint16_t readings[3];  // Buffer for averaging
    static uint8_t read_index = 0;
    
    // Select channel
    ADMUX = (ADMUX & 0xF8) | (channel & 0x07);
    
    // Start conversion
    ADCSRA |= (1 << ADSC);
    while(ADCSRA & (1 << ADSC));
    
    // Store reading in buffer
    readings[read_index] = ADC;
    read_index = (read_index + 1) % 3;
    
    // Calculate average
    uint16_t sum = 0;
    for(uint8_t i = 0; i < 3; i++) {
        sum += readings[i];
    }
    uint16_t current_value = sum / 3;
    
    // Add stability check
    if(abs(current_value - last_value) < 15) {
        stable_count++;
        if(stable_count >= 2) {
            last_value = current_value;
            return current_value;
        }
    } else {
        stable_count = 0;
        last_value = current_value;
    }
    
    return last_value;
}

// Button Functions
void button_init(void) {
    // Configure operator buttons
    DDRB &= ~((1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB3) | 
              (1 << PB4) | (1 << PB5));
    PORTB |= ((1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB3) | 
              (1 << PB4) | (1 << PB5));
              
    // Configure function buttons (analog pins as digital inputs)
    DDRC &= ~((1 << PC1) | (1 << PC2) | (1 << PC3) | (1 << PC4) | (1 << PC5));
    PORTC |= ((1 << PC1) | (1 << PC2) | (1 << PC3) | (1 << PC4) | (1 << PC5));
}

// Derivative function: d^2y/dx^2 = -y
double f(double x, double y, double dy) {
    return -y;
}

// RK4 step for a second-order ODE
void rk4_step(double (*f)(double, double, double), double x, double *y, double *dy, double h) {
    double k1_y = *dy, k1_dy = f(x, *y, *dy);
    double k2_y = *dy + 0.5 * h * k1_dy, k2_dy = f(x + 0.5 * h, *y + 0.5 * h * k1_y, *dy + 0.5 * h * k1_dy);
    double k3_y = *dy + 0.5 * h * k2_dy, k3_dy = f(x + 0.5 * h, *y + 0.5 * h * k2_y, *dy + 0.5 * h * k2_dy);
    double k4_y = *dy + h * k3_dy, k4_dy = f(x + h, *y + h * k3_y, *dy + h * k3_dy);

    *y += (h / 6.0) * (k1_y + 2 * k2_y + 2 * k3_y + k4_y);
    *dy += (h / 6.0) * (k1_dy + 2 * k2_dy + 2 * k3_dy + k4_dy);
}

// Compute sine using RK4
double sine_rk4(double x, double h) {
    double y = 0.0, dy = 1.0; // Initial conditions: y(0) = 0, dy/dx(0) = 1
    for (double t = 0.0; t < x; t += h) {
        rk4_step(f, t, &y, &dy, h);
    }
    return y;
}

// Function to compute tangent using RK4
double tangent_rk4(double radians, double h) {
    // Compute sine and cosine using RK4
    double sine_value = sine_rk4(radians, h);
    double cosine_radians = radians + M_PI / 2; // Add π/2 radians (90 degrees)
    cosine_radians = reduce_angle(cosine_radians); // Reduce to [0, 2π)
    double cosine_value = sine_rk4(cosine_radians, h);

    // Handle division by zero (or very close to zero)
    if (fabs(cosine_value) < 1e-10) {
        return NAN; // Return NaN (Not a Number) for undefined cases
    }

    return sine_value / cosine_value;
}

// Convert degrees to radians
double deg2rad(double deg) {
    return deg * M_PI / 180.0;
}

// Reduce angle to [0, 2π)
double reduce_angle(double rad) {
    if (rad < 0) rad += 2 * M_PI * ceil(fabs(rad) / (2 * M_PI));
    return fmod(rad, 2 * M_PI);
}

// Function to compute x^n using Euler's Method on dy/dx = n*y
double power(double x, double n) {
    if (x <= 0) return -1;  // Only valid for positive x

    double y = 1.0;  // Initial condition: y(0) = 1
    double log_x = log(x);  // Compute ln(x)
    int steps = abs(log_x / STEP_SIZE);  // Compute number of steps dynamically

    double dx = log_x / steps;  // Adjust step size to match ln(x)


    for (int i = 0; i < steps; i++) {
        y += dx * (n * y);  // Euler's method: y_k+1 = y_k + h*f(x, y)
    }

    return y;
}

// Trapezoidal Rule for numerical integration
double trapezoidal_rule(double a, double b, int n) {
    double h = (b - a) / n;  // Step size
    double sum = 0.5 * ((1/a) + (1/b));  // First and last terms

    // Sum intermediate terms
    for (int i = 1; i < n; i++) {
        double x = a + i * h;
        sum += 1/x;
    }

    return h * sum;  // Final result
}

// Function to compute ln(x) using numerical integration
double compute_ln(double x) {
    if (x <= 0) {
        printf("Error: ln(x) is undefined for x <= 0.\n");
        return NAN;  // Return NaN for invalid input
    }

    int n = 1000;  // Number of intervals for integration
    return trapezoidal_rule(1.0, x, n);  // Integrate from 1 to x
}

int gcd(int a, int b) {
    while (b != 0) {
        int temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}

// Function to convert decimal to fraction
void decimal_to_fraction(double decimal, int *numerator, int *denominator) {
    double tolerance = 1e-6;  // Precision control
    int max_denominator = 10000;  // Limit denominator size

    *numerator = 1;
    *denominator = 1;

    // If decimal is zero, return 0/1
    if (decimal == 0) {
        *numerator = 0;
        *denominator = 1;
        return;
    }

    int sign = (decimal < 0) ? -1 : 1;
    decimal = fabs(decimal);

    int temp_denominator = 1;
    while (fabs(decimal * temp_denominator - round(decimal * temp_denominator)) > tolerance && temp_denominator < max_denominator) {
        temp_denominator *= 10;
    }

    *numerator = round(decimal * temp_denominator) * sign;
    *denominator = temp_denominator;

    // Simplify using GCD
    int common_divisor = gcd(*numerator, *denominator);
    *numerator /= common_divisor;
    *denominator /= common_divisor;
}


// Operator precedence
int precedence(const char* op) {
    if(strcmp(op, "e") == 0 || strcmp(op, "p") == 0 || strcmp(op, "A") == 0) return 5;
    if (strcmp(op, "**") == 0) return 4;
    if (strcmp(op, "s") == 0 || strcmp(op, "c") == 0 || strcmp(op, "t") == 0) return 3;
    if (strcmp(op, "*") == 0 || strcmp(op, "/") == 0) return 2;
    if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0) return 1;
    return 0;
}

// Convert Infix to Postfix
void infixToPostfix(const char* infix, char* postfix) {
    Stack s;
    initStack(&s);
    int j = 0;

    for (int i = 0; infix[i] != '\0'; i++) {
        char c = infix[i];
        
        if (isdigit(c) || c == '.') {
            while (isdigit(infix[i]) || infix[i] == '.') {
                postfix[j++] = infix[i++];
            }
            postfix[j++] = ' ';
            i--;
        } 
        else if (c == '(') {
            push(&s, "(");
        } 
        else if (c == ')') {
            while (!isEmpty(&s) && strcmp(peek(&s), "(") != 0) {
                strcpy(&postfix[j], peek(&s));
                j += strlen(peek(&s));
                postfix[j++] = ' ';
                pop(&s);
            }
            pop(&s);
        }
        else if(c == 'e'){
            push(&s, "e");
        }
        else if(c == 'p'){
            push(&s, "p");
        }
        else if (c == 's' && infix[i + 1] == '(') {
            push(&s, "s");
        }
        else if(c == 'l' && infix[i + 1] == '(') {
            push(&s, "l");
        }
        else if(c == 'L' && infix[i + 1] == '(') {
            push(&s, "L");
        }
        else if(c == 'A'){
            push(&s, "A");
        }
        else {
            char op[3] = {c, '\0', '\0'};
            if (infix[i] == '*' && infix[i + 1] == '*') {
                op[1] = '*';
                i++;
            }
            while (!isEmpty(&s) && precedence(peek(&s)) >= precedence(op)) {
                strcpy(&postfix[j], peek(&s));
                j += strlen(peek(&s));
                postfix[j++] = ' ';
                pop(&s);
            }
            push(&s, op);
        }
    }
    while (!isEmpty(&s)) {
        strcpy(&postfix[j], peek(&s));
        j += strlen(peek(&s));
        postfix[j++] = ' ';
        pop(&s);
    }
    postfix[j] = '\0';
}

float evaluatePostfix(const char* postfix) {
    NumStack s;
    initNumStack(&s);
    int i = 0;
    
    while (postfix[i] != '\0') {
        if (isdigit(postfix[i]) || postfix[i] == '.') {
            pushNum(&s, atof(&postfix[i]));
            while (isdigit(postfix[i]) || postfix[i] == '.') i++;
        } 
        else if (postfix[i] == 'e') {
            popNum(&s);  // Remove unused value
            pushNum(&s, 2.71828);
        }
        else if (postfix[i] == 'p') {
            popNum(&s);  // Remove unused value
            pushNum(&s, 3.14159);
        }
        else if (postfix[i] == 's') {
            float val = popNum(&s);
            pushNum(&s, sine_rk4(reduce_angle(deg2rad(val)), STEP_SIZE));
        }
        else if (postfix[i] == 'c') {
            float val = popNum(&s);
            pushNum(&s, sine_rk4(reduce_angle(90 - deg2rad(val)), STEP_SIZE));
        }
        else if (postfix[i] == 't') {
            float val = popNum(&s);
            pushNum(&s, tangent_rk4(deg2rad(val), STEP_SIZE));
        }
        else if (postfix[i] == 'l') {
            float val = popNum(&s);
            pushNum(&s, compute_ln(val));
        }
        else if(postfix[i] == 'L') {
            float val = popNum(&s);
            pushNum(&s, compute_ln(val) / 2.30258);
        }
        else if(postfix[i] == 'A') {
            popNum(&s);  // Remove unused value
            pushNum(&s, temp);
        }
        else if (postfix[i] == '+') {
            float b = popNum(&s);
            float a = popNum(&s);
            pushNum(&s, a + b);
        }
        else if (postfix[i] == '-') {
            float b = popNum(&s);
            float a = popNum(&s);
            pushNum(&s, a - b);
        }
        else if (postfix[i] == '*') {
            float b = popNum(&s);
            float a = popNum(&s);
            pushNum(&s, a * b);
        }
        else if (postfix[i] == '/') {
            float b = popNum(&s);
            float a = popNum(&s);
            pushNum(&s, a / b);
        }
        i++;
    }
    return popNum(&s);
}

int main(void) {
    // Initialize peripherals
    lcd_init();
    adc_init();
    button_init();
    
    // Initial display
    lcd_clear();
    lcd_print("Enter: ");
    
    char buffer[20];  // Buffer for number conversion
    uint16_t last_adc_value = 0;
    char last_char = '_';
    uint8_t debounce_counter = 0;
    uint8_t display_update_counter = 0;
    uint8_t button_state = 0xFF;  // All buttons initially released
    
    while(1) {
        // Read ADC values for number pad and function buttons
        uint16_t numpad_value = adc_read(NUM_PAD_ADC);
        uint16_t func1_value = adc_read(FUNC1_ADC);
        uint16_t func2_value = adc_read(FUNC2_ADC);
        char selectedChar = '_';
        
        // Process number pad ADC value
        if(numpad_value > 900) {  // No button pressed
            selectedChar = '_';
        }
        else if(numpad_value > 850) selectedChar = '0';
        else if(numpad_value > 800) selectedChar = '1';
        else if(numpad_value > 750) selectedChar = '2';
        else if(numpad_value > 700) selectedChar = '3';
        else if(numpad_value > 650) selectedChar = '4';
        else if(numpad_value > 600) selectedChar = '5';
        else if(numpad_value > 550) selectedChar = '6';
        else if(numpad_value > 500) selectedChar = '7';
        else if(numpad_value > 450) selectedChar = '8';
        else if(numpad_value > 400) selectedChar = '9';
        
        // Process function button A1 (sin, cos, tan)
        if(func1_value < 300 && last_func1_press == 0) {  // Button pressed
            last_func1_press = 1;
            func1_state = (func1_state + 1) % 4;  // Cycle through 0,1,2,3
            
            // Clear previous input and add selected function
            inputPos = 0;
            for(int i=0; i<50; i++) {
                userInput[i] = '\0';
            }
            
            switch(func1_state) {
                case 1:
                    userInput[inputPos++] = 's';
                    userInput[inputPos++] = '(';
                    num_brackets++;
                    break;
                case 2:
                    userInput[inputPos++] = 'c';
                    userInput[inputPos++] = '(';
                    num_brackets++;
                    break;
                case 3:
                    userInput[inputPos++] = 't';
                    userInput[inputPos++] = '(';
                    num_brackets++;
                    break;
            }
            
            // Update display immediately
            lcd_clear();
            lcd_set_cursor(0, 0);
            lcd_print("Enter: ");
            lcd_set_cursor(0, 1);
            lcd_print(userInput);
            debounce_counter = 5;
        }
        else if(func1_value > 800) {  // Button released
            last_func1_press = 0;
        }
        
        // Process function button A2 (ln, log10)
        if(func2_value < 300 && last_func2_press == 0) {  // Button pressed
            last_func2_press = 1;
            func2_state = (func2_state + 1) % 3;  // Cycle through 0,1,2
            
            // Clear previous input and add selected function
            inputPos = 0;
            for(int i=0; i<50; i++) {
                userInput[i] = '\0';
            }
            
            switch(func2_state) {
                case 1:
                    userInput[inputPos++] = 'l';
                    userInput[inputPos++] = '(';
                    num_brackets++;
                    break;
                case 2:
                    userInput[inputPos++] = 'L';
                    userInput[inputPos++] = '(';
                    num_brackets++;
                    break;
            }
            
            // Update display immediately
            lcd_clear();
            lcd_set_cursor(0, 0);
            lcd_print("Enter: ");
            lcd_set_cursor(0, 1);
            lcd_print(userInput);
            debounce_counter = 5;
        }
        else if(func2_value > 800) {  // Button released
            last_func2_press = 0;
        }
        
        // Read button states for operator buttons
        uint8_t current_button_state = PINB;
        
        // Process operator button changes
        if(debounce_counter == 0) {
            // Process operator buttons
            if((current_button_state & (1 << OPERATOR1_BUTTON)) != (button_state & (1 << OPERATOR1_BUTTON))) {
                if(!(current_button_state & (1 << OPERATOR1_BUTTON))) {
                    selectedChar = '+';
                    debounce_counter = 5;
                }
            }
            else if((current_button_state & (1 << OPERATOR2_BUTTON)) != (button_state & (1 << OPERATOR2_BUTTON))) {
                if(!(current_button_state & (1 << OPERATOR2_BUTTON))) {
                    selectedChar = '-';
                    debounce_counter = 5;
                }
            }
            else if((current_button_state & (1 << OPERATOR3_BUTTON)) != (button_state & (1 << OPERATOR3_BUTTON))) {
                if(!(current_button_state & (1 << OPERATOR3_BUTTON))) {
                    selectedChar = '*';
                    debounce_counter = 5;
                }
            }
            else if((current_button_state & (1 << OPERATOR4_BUTTON)) != (button_state & (1 << OPERATOR4_BUTTON))) {
                if(!(current_button_state & (1 << OPERATOR4_BUTTON))) {
                    selectedChar = '/';
                    debounce_counter = 5;
                }
            }
            // Check for parentheses and enter buttons
            else if((current_button_state & (1 << PAREN_BUTTON)) != (button_state & (1 << PAREN_BUTTON))) {
                if(!(current_button_state & (1 << PAREN_BUTTON))) {
                    buttonPressTime = currentTime;
                } else {
                    if(buttonPressTime > 0) {
                        if(currentTime - buttonPressTime > BUTTON_HOLD_TIME) {
                            selectedChar = ')';
                            num_brackets--;
                        } else {
                            selectedChar = '(';
                            num_brackets++;
                        }
                        debounce_counter = 5;
                    }
                    buttonPressTime = 0;
                }
            }
            else if((current_button_state & (1 << ENTER_BUTTON)) != (button_state & (1 << ENTER_BUTTON))) {
                if(!(current_button_state & (1 << ENTER_BUTTON))) {
                    if(inputPos == 0) {
                        int num, den;
                        decimal_to_fraction(temp, &num, &den);
                        lcd_clear();
                        _delay_ms(50);
                        lcd_print(itoa(num, buffer, 10));
                        lcd_print("/");
                        lcd_print(itoa(den, buffer, 10));
                    } else {
                        char postfix[50];
                        if(num_brackets > 0) {
                            for(int i=inputPos; i<num_brackets + inputPos; i++) {
                                userInput[i] = ')';
                            }
                        }
                        infixToPostfix(userInput, postfix);
                        float result = evaluatePostfix(postfix);
                        temp = result;
                        lcd_clear();
                        lcd_print(itoa(result, buffer, 10));
                        inputPos = 0;
                        for(int i=0; i<50; i++) {
                            userInput[i] = '\0';
                        }
                    }
                    debounce_counter = 5;
                }
            }
        }
        
        button_state = current_button_state;
        
        // Process input and update display
        if(selectedChar != '_' && selectedChar != last_char) {
            if(inputPos < 16) {
                userInput[inputPos] = selectedChar;
                inputPos++;
            }
        }
        last_char = selectedChar;
        
        // Update display
        if(display_update_counter == 0) {
            lcd_set_cursor(0, 0);
            lcd_print("Enter: ");
            lcd_set_cursor(0, 1);
            lcd_print(userInput);
            display_update_counter = 2;
        }
        
        // Update counters
        currentTime++;
        if(debounce_counter > 0) debounce_counter--;
        if(display_update_counter > 0) display_update_counter--;
        _delay_ms(1);
    }
    
    return 0;
}
