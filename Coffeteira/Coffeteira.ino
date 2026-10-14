/* ===========================================================================
   Universidade Tecnológica Federal do Paraná

   Trabalho de Sistemas Embarcados

   Máquina de Café

   Acadêmicos:  Alisson
                Isadora
                Lucas
                William
   =========================================================================== */
// Inclusão das bibliotecas
#include <LCD.h>
#include <LiquidCrystal_I2C.h>
#include <Thermistor.h>
#include <Wire.h>
#include <PIDController.h>
#include "RTClib.h"

// ----------------------------------------------------------------------------------------------------------------------------------------------------------
// Para teste, descomentar a linha abaixo.
//#define teste

//Inicializa o display no endereco 0x27
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE); //ENDEREÇO DO I2C E DEMAIS INFORMAÇÕES

RTC_DS3231 rtc;   // OBJETO DO TIPO RTC_DS3231

// ===========================================================================
// --- Mapeamento de Hardware ---
// Definição dos botões de ajuste
//#define sTpt        A0      // Pino de entrada para a leitura do sensor de temperatura, parâmetro é o pino ADC.
Thermistor sTpt(0);
#define BTN_1 (1<<PC7)      // Ajuste hora - Pino 30
#define BTN_2 (1<<PC6)      // Ajuste minuto - Pino 31
#define BTN_3 (1<<PC5)      // Ajuste do botão para programar - Pino 32
#define BTN_4 (1<<PC4)      // Intensidade do café (normal/forte) - Pino 33 
#define BTN_5 (1<<PC3)      // Ligar - Agora/Programado - Pino 34
#define pulse (1<<PD3)      // Interrupção externa - pino 18
// Definição dos pinos para o ultrassom
#define echoPin 52          // define o pino D52 do Arduino como pino Echo do HC-SR04
#define trigPin 53          // define o pino D53 do Arduino como pino Triger do HC-SR04
// Definição dos pinos para a temperatura
#define Sensor  A0          // define o pino A0 como entrada analógica para leitura da temperatura
// Definição dos pinos para o sensor de peso

// Definição dos pinos para o aquecedor
#define atuaAquec 4         // Pino de saída para acionar o aquecedor via PWM.
// Definição dos pinos para os LEDs
#define Normal  37          // Led 1 - Intensidade normal do café - pino 37
#define Forte   36          // Led 2 - Intensidade forte do café - pino 36
#define LED_2   42
#define LED_3   43
// ===========================================================================
// --- Protótipo das Funções ---
void ReadButton();
void setHoraMin();
void setAlarme();
short IntesidadeCoffee();
void menuCoffee();
void PrepararCoffee(byte Ligado, byte Aguarda);
boolean read_Temperature(int temp);
boolean readUltrasound(int dist);
void displaySetTimer();
void reset();
void bakclight();
// ===========================================================================
// --- Variáveis Globais ---
boolean  opcao_1 = 0x00;    // Ajuste hora - Pino 30
boolean  opcao_2 = 0x00;    // Ajuste minuto - Pino 31
boolean  opcao_3 = 0x00;    // Ajuste do botão para programar - Pino 32
boolean  opcao_4 = 0x00;    // Intensidade do café (normal/forte) - Pino 33
boolean  opcao_5 = 0x00;    // Ligar - Agora/Programado - Pino 34
byte bt_press = 0;          // Variável para auxiliar na escolha da rotina

// Relacionados ao PID (Os valores foram atribuidos de forma aleatória)
#define __Kp 30     // Proportional constant
#define __Ki 0.7    // Integral Constant
#define __Kd 200    // Derivative Constant

PIDController pid;  // Create an instance of the PID controller class, called "pid"

byte hora = 00;         // Variável para controle das horas do alarme
byte minuto = 00;       // Variável para controle dos minutos do alarme
byte tempAlvo;          // Temperatura alvo

double temperatura = 0;         // Armazena a temperatura lida pelo sensor
short full;                     // Define a distância em cm, a distância do sensor até o pó de café
long duration;                  // Variável para duração que a onda de som viaja
short distance = 00;            // Variável para medição da distancia
boolean auxUltra;               // Variável auxiliar para a leitura da presença de café por meio do sensor de ultrassom

boolean Programar   = 0x00;     // Variável auxiliar para escolher set do horário ou do alarme
boolean select = 0x01;          // Varíavel que auxilia na escolha da intensidade do café
short Intensidade = 1;          // Variável para identificação da intesidade do café (Normal - 1 | Forte - 2)
short Selecionar;               // Variável auxiliar para a escolha do preparo do café
short cont;                     // Variável auxiliar
float timerIN;                  // Variável que grava o tempo em que a Coffeteira ligou
float timerOUT;                 // Variável que grava o tempo em que a água começou a ser aqucida

//Definições para o o fundo escuro
short setLight_ini = 60;
short setLight = setLight_ini;
short backlight = 1;
// ========================================================================================================
// --- Interrupção ---
ISR (INT2_vect)       // Vetor para interrupção externa 2
{

  switch (bt_press)
  {
    case 1:
      opcao_1 = 0x01; // Irá incrementar a hora
      break;
    case 2:
      opcao_2 = 0x01; // Irá incremetnar o minuto
      break;
    case 3:
      opcao_3 = 0x01; // Habilita a programação do horário de preparo
      break;
    case 4:
      opcao_4 = 0x01; // Altera a opção do café (noraml/forte)
      break;
    case 5:
      opcao_5 = 0x01; // Prepara o café (Agora/Programado/Standby)
      break;
  } //end switch bt_press

} //end ISR
// -----------------------------------------------------------------------------------------------------------------
// --- Programa Principal ---
void setup()
{
  Serial.begin(115200);
  lcd.begin(16, 2);
  Wire.begin();         // Chama a biblioteca Wire

  DDRE &= ~BTN_1;       // Ajuste hora - Pino 30
  DDRE &= ~BTN_2;       // Ajuste minuto - Pino 31
  DDRB &= ~BTN_3;       // Ajuste do botão para programar - Pino 32
  DDRB &= ~BTN_4;       // Intensidade do café (normal/forte) - Pino 33
  DDRB &= ~BTN_5;       // Ligar - Agora/Programado - Pino 34

  pinMode(Normal, OUTPUT);          // Configura o pino como saída
  pinMode(Forte , OUTPUT);          // Configura o pino como saída
  pinMode(LED_2 , OUTPUT);          // Configura o pino como saída
  pinMode(LED_3 , OUTPUT);          // Configura o pino como saída
  pinMode(atuaAquec, OUTPUT);       // Declaração do pino como saída

  // Configura pino de Trigger como saída e inicializa com nível baixo
  pinMode(trigPin, OUTPUT);
  digitalWrite(trigPin, LOW);
  pinMode(echoPin, INPUT); // configura pino ECHO como entrada

  // Condições inicias da Coffeteira
  full = 12;                        // Distância para a validação da existência de pó de café no filtro
  digitalWrite(Normal, HIGH);       // O Set da máquina é para fazer café normal
  select = 0x01;
  Intensidade = 1;
  digitalWrite(atuaAquec, LOW);     // Inicia o aquecedor desligado
  tempAlvo = 60;                    // Temperatura alvo quando o sistema é iniciado
  Selecionar = 1;
  cont = 1;
  Programar = 0x00;
  auxUltra  = 0x00;

  DDRD  |=  pulse;        // Configura saída para o pulso de interrupção externa
  PORTD &= ~pulse;        // Inicializa pulse em LOW

  DDRD  &= ~(1 << PD2);                     // Configura entrada para int2
  PORTD |= (1 << PORTD2);                   // Habilita pull-up interno de PD2

  EICRA |= (1 << ISC21) | (1 << ISC20);     // Configura interrupção externa 0 como borda de subida
  EIMSK |= (1 << INT2);                     // Habilita interrupção externa 0

  sei();                                    // Liga interrupções

  lcd.setBacklight(HIGH);

  //rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); //CAPTURA A DATA E HORA EM QUE O SKETCH É COMPILADO
  //rtc.adjust(DateTime(2022, 04, 01, 14, 38, 00)); //(ANO), (MÊS), (DIA), (HORA), (MINUTOS), (SEGUNDOS)

  //---------------------------------PID-----------------------------------------
  pid.begin();                  // Inicialize a instância PID
  pid.tune(__Kp, __Ki, __Kd);   // Argumentos para o ajuste PID : kP, kI, kD
  pid.limit(0, 255);            // Os limites de saída do PID variam entre 0 e 255
  //-----------------------------------------------------------------------------
#if defined(teste)
  Serial.println("-------- Sistema Pronto --------");
  Serial.println("--------------------------------");
#endif

} // end setup
// -----------------------------------------------------------------------------------------------------------------
void loop()
{
  timerIN = millis();   // Grava o tempo em que o sistema começou a funcionar
  displaySetTimer();    // Chama a função que imprime as informações no display
  ReadButton();         // Chama a função de leitura dos botões

  // Caso o botão 1 ou 2 seja pressionado, ajuste de hora ou minuto
  if (opcao_1 || opcao_2)
  {
    if (!Programar)
      setHoraMin();       // Altera hora ou minuto do horário atual
    else if (Programar)
      setAlarme();        // Altera hora ou minuto do alarme
  }
  else if (opcao_3)       // Ajuste do botão para programar
  {
    // Se "Programar" for igual a 0x01 o alarme poderá ser ajustado
    // E se "Programar for igual a 0x00 o horário poderá ser ajustado.
    Programar = !Programar;
    opcao_3 = 0x00;
  }
  else if (opcao_4)       // Ajusta a intensidade do café
  {
    Intensidade = IntesidadeCoffee();       // Se o retorno for 1, o café será normal
    if (Intensidade == 1)                   // e se o retorno for 2, o café será forte
      tempAlvo = 60;
    else if (Intensidade == 2)
      tempAlvo = 80;
    opcao_4 = 0x00;
  }
  else if (opcao_5)        // Ligar agora/programado/standby
  {
    cont++;
    // Se "Selecionar" for igual a 3 o café será feito com um horário pré programado
    // se "Selecionar" for igual a 2 o café será feito imediatamente e se "Selecionar" for igual a 1 a cafeteira permanecerá desligada
    if (cont == 1)
      Selecionar = 1;         // Cafeteira em espera

    else if (cont == 2)
      Selecionar = 2;         // Café será feito imediatamente

    else if (cont == 3)
    {
      Selecionar = 3;         // Café será feito no horário programado
      cont = 0;
    }
    opcao_5 = 0x00;
    auxUltra = 0;
    
  } // end if opcao_5

  menuCoffee();               // Chama a rotina "menuCoffee"

} // end loop
// -----------------------------------------------------------------------------------------------------------------
void ReadButton()
{
  static boolean  bt1_f = 0x00,
                  bt2_f = 0x00,
                  bt3_f = 0x00,
                  bt4_f = 0x00,
                  bt5_f = 0x00;

  if (!(PINC & BTN_1)) bt1_f = 0x01;
  if (!(PINC & BTN_2)) bt2_f = 0x01;
  if (!(PINC & BTN_3)) bt3_f = 0x01;
  if (!(PINC & BTN_4)) bt4_f = 0x01;
  if (!(PINC & BTN_5)) bt5_f = 0x01;

  if ((PINC & BTN_1) && bt1_f)
  {
    bt1_f = 0x00;
    PORTD |= pulse;
    bt_press = 1;
    PORTD &= ~pulse;

  } //end if bt1

  if ((PINC & BTN_2) && bt2_f)
  {
    bt2_f = 0x00;
    PORTD |= pulse;
    bt_press = 2;
    PORTD &= ~pulse;

  } //end if bt2

  if ((PINC & BTN_3) && bt3_f)
  {
    bt3_f = 0x00;
    PORTD |= pulse;
    bt_press = 3;
    PORTD &= ~pulse;

  } //end if bt3

  if ((PINC & BTN_4) && bt4_f)
  {
    bt4_f = 0x00;
    PORTD |= pulse;
    bt_press = 4;
    PORTD &= ~pulse;

  } //end if bt4

  if ((PINC & BTN_5) && bt5_f)
  {
    bt5_f = 0x00;
    PORTD |= pulse;
    bt_press = 5;
    PORTD &= ~pulse;

  } //end if bt5

} // end ReadButton
// -----------------------------------------------------------------------------------------------------------------
void setHoraMin()
{
  DateTime now = rtc.now();

  byte aux_hra = now.hour();
  byte aux_min = now.minute();
  byte aux_sec = now.second();

  if (opcao_1) aux_hra++;
  else if (opcao_2) aux_min++;

  if (aux_hra == 24) aux_hra = 00;
  if (aux_min == 59) aux_min = 00;

  rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); //CAPTURA A DATA E HORA EM QUE O SKETCH É COMPILADO
  rtc.adjust(DateTime(2022, 04, 01, aux_hra, aux_min, aux_sec)); //(ANO), (MÊS), (DIA), (HORA), (MINUTOS), (SEGUNDOS)
  /*Como a data não é importante para o nosso sistema, foi utilizada a data em que o código foi elaborado.  */

  opcao_1 = 0x00;
  opcao_2 = 0x00;

} // end setHoraMin
// -----------------------------------------------------------------------------------------------------------------
void setAlarme()
{
  if (opcao_1) hora++;
  else if (opcao_2)  minuto++;

  if (hora == 24) hora = 00;
  if (minuto == 59) minuto = 00;

  opcao_1 = 0x00;
  opcao_2 = 0x00;
} // end setAlarme
// -----------------------------------------------------------------------------------------------------------------
short IntesidadeCoffee()
{
  if (!select)
  {
    digitalWrite(Normal, HIGH);
    digitalWrite(Forte , LOW);
    select = 0x01;

    return 1;
  }
  else if (select)
  {
    digitalWrite(Normal, LOW);
    digitalWrite(Forte , HIGH);
    select = 0x00;

    return 2;
  }
} // end Intensidade
// -----------------------------------------------------------------------------------------------------------------
void menuCoffee()
{
  if (Selecionar == 1)
  {
    digitalWrite(LED_2, LOW );
    digitalWrite(LED_3, LOW );
    digitalWrite(atuaAquec, LOW);     // Inicia o aquecedor desligado
  }


  else if (Selecionar == 2)
  {
    digitalWrite(LED_2, HIGH);   
    PrepararCoffee(LED_2, LED_3);
  }
  else if (Selecionar == 3)
  {
    digitalWrite(LED_3, HIGH);
    digitalWrite(atuaAquec, LOW );
    DateTime now = rtc.now();

    byte aux_hra = now.hour();
    byte aux_min = now.minute();

    // Aguarda o horário programado
    if (aux_hra == hora && aux_min == minuto)
      PrepararCoffee(LED_3, LED_2);
  }
} // end menuCoffee
// -----------------------------------------------------------------------------------------------------------------
void PrepararCoffee(byte Ligado, byte Aguarda)
{
  byte output;

  if (!auxUltra)
  {
    // Testa se há café no reservatório
    auxUltra = readUltrasound(full);       // Deve-se passar a distância desejada: 12cm
    timerOUT = millis();
  }

  digitalWrite(Ligado, HIGH);
  
  if (auxUltra == true)
  {
    digitalWrite(Aguarda, LOW);
    // Intensidade do café - Normal ou Forte
    pid.setpoint(tempAlvo);

    // Lê temperatura
    temperatura = sTpt.getTemp();

    // Controle da temperatura (sTpt)
    output = pid.compute(temperatura);      // Deixe o PID calcular o valor e retorna a saída ideal (0 a 255)
#if defined(teste)
    Serial.print("Temperatura: ");
    Serial.println(temperatura);
    Serial.print("PWM: ");
    Serial.println(output);
#endif
    // Aciona aquecedor via PWM
    analogWrite(atuaAquec, output);         // Escreve a saída PWM no pino 4 do Arduino Mega
  }
  else
    digitalWrite(Aguarda, HIGH);
  
  // Tempo escolhido para o preparo do café
  if ( (timerIN - timerOUT) >= 10000 )      // O tempo escolhido foi de 10s para que a lógica pudesse ser apresentada em tempo hábil ao longo na aula
  {
    reset();                                // Após o preparo do café, a Coffeteira é setada e fica em espera (standby)
  }

} // end PrepararCoffee
// -----------------------------------------------------------------------------------------------------------------
boolean readUltrasound(int dist)
{
  //ENQUANTO A DISTÂNCIA DO CAFÉ NÃO ESTIVER DE ACORDO COM O PARÂMETRO COLOCADO

  // SETA COMO NÍVEL BAIXO, trigPin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  // SETA COMO NÍVEL ALTO (ACTIVE) POR 10 MICROSEGUNDOS
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  // Le o echoPin, retorna o tempo de viagem da onda sonora em microssegundos
  duration = pulseIn(echoPin, HIGH);
  // CALCULA A DISTÂNCIA
  distance = duration * 0.034 / 2; // VELOCIDADE DO SOM DIVIDIDO POR 2 (IDA E VOLTA)

  if (dist >= distance )
    return true;  // Retorna verdadeiro quando há café no recipiente
  else
    return false; // Retorna falso quando não há café no recipiente

} // end readUltrasound
// -----------------------------------------------------------------------------------------------------------------
void displaySetTimer()
{
  byte aux_hra;
  byte aux_min;

  lcd.setCursor(1, 1); // linha 2
  if (Programar)
  {
    lcd.print("Programar!   ");

    aux_hra = hora;
    aux_min = minuto;
  }
  else if (!Programar)
  {
    DateTime now = rtc.now();

    lcd.print("Horario Atual");

    aux_hra = now.hour();
    aux_min = now.minute();
  }

  // Mostra TIME em horas, minuto e segundo + AM/PM
  lcd.setCursor(0, 0);
  lcd.print("Hora: ");
  if (aux_hra < 10)lcd.print("0"); // sempre 2 dígitos
  lcd.print(aux_hra);
  lcd.print(":");
  if (aux_min < 10)lcd.print("0");
  lcd.print(aux_min);

} // end displaySetTimer
// -----------------------------------------------------------------------------------------------------------------
void bakclight()
{
  setLight--;
  if (setLight == 0) {
    lcd.setBacklight(LOW);
    backlight = 0;
  }
} // end bakclight
// -----------------------------------------------------------------------------------------------------------------
void reset()
{
  // Set da máquina
  digitalWrite(LED_2, LOW );
  digitalWrite(LED_3, LOW );
  digitalWrite(atuaAquec, LOW);     // Inicia o aquecedor desligado
  Selecionar = 1;
  cont = 1;
  auxUltra  = 0x00;
} // end reset
// -----------------------------------------------------------------------------------------------------------------
