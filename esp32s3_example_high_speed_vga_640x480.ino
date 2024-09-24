#include <Arduino.h>
#include "VGA.h"
#include "CodePage437_8x14.h"

VGA vga;

int taskData[2][3] = 
  {
    {0, 0, 320},
    {0, 320, 640} 
  };

static float v = -1.5;
static float vs = 0.001;

//https://en.wikipedia.org/wiki/Julia_set#Pseudocode_for_normal_Julia_sets
int julia(int x, int y, float cx, float cy)
{
  int zx = ((x - 319.5f) * (1.f / 640.f * 5.0f)) * (1 << 12);
  int zy = ((y - 239.5f) * (1.f / 480.f * 3.0f)) * (1 << 12);
  int i = 0;
  const int maxi = 20;
  int cxi = cx ;
  int cyi = cy * (1 << 12);
  while(zx * zx + zy * zy < (4 << 24) && i < maxi) 
  {
    int xtemp = (zx * zx - zy * zy) >> 12;
    zy = ((zx * zy) >> 11) + cyi; 
    zx = xtemp + cxi;
    i++;
  }
  return i;
}

int colors[] = {
  0b0000000000010000, //10
  0b0000000000011000, //11
  0b1000000000010000, //12
  0b1000000000011000, //13
  0b1100000000011000, //14
  0b1000010000011000, //15
  0b1000011000010000, //16
  0b1100010000010000, //17
  0b1000010000011000, //18
  0b1100011000011110,  //19
  0b1000000000000000, //0 
  0b1100000000000000, //1
  0b1000010000000000, //2
  0b1100010000000000, //3
  0b1000011000000000, //4
  0b1100011000000000, //5
  0b0000010000000000, //6
  0b0000011000000000, //7
  0b0000011000010000, //8
  0b0000010000010000, //9
  };

void renderTask(void *param)
{
  int *data = (int*)param;
  while(true)
  {
    while(!data[0]) delay(1);
    for(int y = 0; y < 240; y++)
      for(int x = data[1]; x < data[2]; x++)
      {
        int c = colors[julia(x, y, -0.74543f, v)];
        vga.dot(x, y, c);
        vga.dot(639 - x, 479 - y, c);
      }
    data[0] = 0;
  }
}

void setup() {
  Serial.begin(115200);
  vga.init();
  vga.setFont(CodePage437_8x14);
  vga.setTextColor(65535,0);
  TaskHandle_t xHandle = NULL;
  xTaskCreatePinnedToCore(renderTask, "Render1", 2000, taskData[0],  ( 2 | portPRIVILEGE_BIT ), &xHandle, 0);
  xTaskCreatePinnedToCore(renderTask, "Render2", 2000, taskData[1],  ( 2 | portPRIVILEGE_BIT ), &xHandle, 1);

 
  delay(100);
}

void loop() {
   static unsigned long ot = 0;
  unsigned long t = millis();
  unsigned long dt =  t - ot;
  ot = t;
  taskData[0][0] = 1;
  taskData[1][0] = 1;
  //waiting for task to finish
  while(taskData[0][0] || taskData[1][0]) {delay(2);}
  vga.setCursor(20,50);
  vga.print(1000/(millis()-t));vga.print("Fps");
  v += vs * dt;
  if(v > 1.5f)
  {
    v = 1.5f;
    vs = -vs;
  }
  if(v < -1.5f)
  {
    v = -1.5f;
    vs = -vs;
  }
  vga.vsyncWait();
}
