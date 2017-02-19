#include <RBD_Timer.h>

int n=0;
int Interval=10000;
RBD::Timer timer;

void setup() {
  // put your setup code here, to run once:
Serial.begin(9600);
timer.setTimeout(Interval);

timer.restart();

}

void loop() {
  // put your main code here, to run repeatedly:
if(timer.onRestart()){
n=0;
  Serial.print(Interval/1000);
  Serial.println(" seconds passed");
  delay(1000);
}
n=n+1;
  Serial.print("doing other stuff ");
  Serial.println(n);
}
