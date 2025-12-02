// Definição prévia dos pinos utilizados no arduino
// Caso precise modificar, redefinir apenas este ponto

#define TRIG_PIN 4  
#define ECHO_PIN 3  

// Função responsável por calcular a distância com base em
// pulsos ultrassoniccos do sensor.

long readUltrasonic() {
  
  // O pino de trigger deve iniciar com o valor 0
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
	
  // É feito um pulso de ativação ao pino Trigger, o qual
  // leva ao módulo a emitir uma rajada de 8 pulsos à 40kHz
  // Após isso, o pino fica em estado baixo novamente
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
	
  // Os pulsos viajam pelo ar até encontrar um objeto, o qual
  // reflete esses pulsos ,sendo novamente capturados pelo sensor.
  // O pino ECHO fica em nível alto pelo tempo exato que o 
  //pulso demorou para ir e voltar.
  long duration = pulseIn(ECHO_PIN, HIGH);  
  
  // O ultrassom viaja no ar a aproximadamente: 340 m/s = 0.034 cm/µs
  // Portanto a distância é é metade do tempo de voo do pulso
  long distance = duration * 0.034 / 2;
  return distance; 
}

void setup()
{
  // Inicialização do monitor serial do Arduino (para debug)
  Serial.begin(9600);
  // Definição do comportamento dos pinos
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
}

void loop()
{
  // Chamada da função de leitura da distância
  long d = readUltrasonic();
  // Demonstração do valor calculado no Serial
  Serial.print("Distância: ");
  Serial.println(d);
  // Aguarda 100 millisecond(s) antes da próxima leitura
  delay(100); 
}

