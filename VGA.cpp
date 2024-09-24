#include "VGA.h"

#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_rgb.h>
#include <esp_async_memcpy.h>

const async_memcpy_config_t async_mem_cfg = {
        .backlog = 8,
        .sram_trans_align = 64,
        .psram_trans_align = 64,
        .flags = 0
    };
async_memcpy_t async_mem_handle;

#define VGA_PIN_NUM_HSYNC          20
#define VGA_PIN_NUM_VSYNC          19
#define VGA_PIN_NUM_DE             -1 //LCD only(optional)

#define VGA_PIN_NUM_PCLK           -1  //For LCD needed real IO-pin
#define VGA_PIN_NUM_DATA0          10 //B0
#define VGA_PIN_NUM_DATA1          11 //B1
#define VGA_PIN_NUM_DATA2          12 //B2
#define VGA_PIN_NUM_DATA3          13 //B3
#define VGA_PIN_NUM_DATA4          14 //B4
#define VGA_PIN_NUM_DATA5          16 //G0
#define VGA_PIN_NUM_DATA6          17 //G1
#define VGA_PIN_NUM_DATA7          18 //G2
#define VGA_PIN_NUM_DATA8          8  //G3
#define VGA_PIN_NUM_DATA9          3  //G4
#define VGA_PIN_NUM_DATA10         9  //G5
#define VGA_PIN_NUM_DATA11         4  //R0
#define VGA_PIN_NUM_DATA12         5  //R1
#define VGA_PIN_NUM_DATA13         6  //R2
#define VGA_PIN_NUM_DATA14         7  //R3
#define VGA_PIN_NUM_DATA15         15 //R4

#define VGA_PIN_NUM_DISP_EN        -1 //LCD only
bool VGA::init(){
  return init(640,480);
}
bool VGA::init(int width, int height) {
  esp_async_memcpy_install(&async_mem_cfg,&async_mem_handle);
  _Width = width;
  _Height = height;
  _bounceLinesBuf = height / 10;
  _pixClock = 25000000;
  _sem_vsync_end = xSemaphoreCreateBinary();
  assert(_sem_vsync_end);
  _sem_gui_ready = xSemaphoreCreateBinary();
  assert(_sem_gui_ready);
  esp_lcd_rgb_panel_config_t panel_config;
  memset(&panel_config, 0, sizeof(esp_lcd_rgb_panel_config_t));
  panel_config.data_width = 16;
  panel_config.bits_per_pixel=16;
  //panel_config.psram_trans_align = 64;
  panel_config.dma_burst_size = 64;
  panel_config.num_fbs = 0;
  panel_config.clk_src = LCD_CLK_SRC_PLL240M;
  panel_config.bounce_buffer_size_px = _bounceLinesBuf * _Width;
  panel_config.disp_gpio_num = VGA_PIN_NUM_DISP_EN;
  panel_config.pclk_gpio_num = VGA_PIN_NUM_PCLK;
  panel_config.vsync_gpio_num = VGA_PIN_NUM_VSYNC;
  panel_config.hsync_gpio_num = VGA_PIN_NUM_HSYNC;
  panel_config.de_gpio_num = VGA_PIN_NUM_DE;
  panel_config.data_gpio_nums[0] = VGA_PIN_NUM_DATA0;
  panel_config.data_gpio_nums[1] = VGA_PIN_NUM_DATA1;
  panel_config.data_gpio_nums[2] = VGA_PIN_NUM_DATA2;
  panel_config.data_gpio_nums[3] = VGA_PIN_NUM_DATA3;
  panel_config.data_gpio_nums[4] = VGA_PIN_NUM_DATA4;
  panel_config.data_gpio_nums[5] = VGA_PIN_NUM_DATA5;
  panel_config.data_gpio_nums[6] = VGA_PIN_NUM_DATA6;
  panel_config.data_gpio_nums[7] = VGA_PIN_NUM_DATA7;
  panel_config.data_gpio_nums[8] = VGA_PIN_NUM_DATA8;
  panel_config.data_gpio_nums[9] = VGA_PIN_NUM_DATA9;
  panel_config.data_gpio_nums[10] = VGA_PIN_NUM_DATA10;
  panel_config.data_gpio_nums[11] = VGA_PIN_NUM_DATA11;
  panel_config.data_gpio_nums[12] = VGA_PIN_NUM_DATA12;
  panel_config.data_gpio_nums[13] = VGA_PIN_NUM_DATA13;
  panel_config.data_gpio_nums[14] = VGA_PIN_NUM_DATA14;
  panel_config.data_gpio_nums[15] = VGA_PIN_NUM_DATA15;
  panel_config.timings.pclk_hz = _pixClock;
  panel_config.timings.h_res = _Width;
  panel_config.timings.v_res = _Height;
  panel_config.timings.hsync_back_porch  =48;
  panel_config.timings.hsync_front_porch =16;
  panel_config.timings.hsync_pulse_width =96;
  panel_config.timings.vsync_back_porch  =33;
  panel_config.timings.vsync_front_porch =10;
  panel_config.timings.vsync_pulse_width =2;
  panel_config.timings.flags.pclk_active_neg = false;
  panel_config.timings.flags.hsync_idle_low = 0;
  panel_config.timings.flags.vsync_idle_low = 0;
  panel_config.flags.fb_in_psram = 0;
  panel_config.flags.double_fb = 0;
  panel_config.flags.no_fb = 1;
  esp_lcd_new_rgb_panel(&panel_config, &_panel_handle);
  int fbSize = _Width*_Height*2; //two bytes on pixel (RGB565 pixel format)
  for (int i = 0; i < 2; i++) {
        _frBuf[i] = (uint16_t*)heap_caps_aligned_alloc(64,fbSize,MALLOC_CAP_SPIRAM);
        assert(_frBuf[i]);
  }
  memset(_frBuf[1], 0, fbSize);
  memset(_frBuf[0], 0, fbSize);
  _lastBBuf = _Width*(_Height-_bounceLinesBuf);
  esp_lcd_rgb_panel_event_callbacks_t cbs = {
      .on_vsync = vsyncEvent,
      .on_bounce_empty = bounceEvent,
  };
  esp_lcd_rgb_panel_register_event_callbacks(_panel_handle, &cbs, this);
  esp_lcd_panel_reset(_panel_handle);
  esp_lcd_panel_init(_panel_handle);
  //esp_lcd_panel_disp_on_off(_panel_handle, 1); //LCD only
    return true;
}


void VGA::vsyncWait() {
    xSemaphoreGive(_sem_gui_ready);
    xSemaphoreTake(_sem_vsync_end, 1);//portMAX_DELAY);
}

uint16_t* VGA::getDrawBuffer() {
  if (_fbIndex == 1) {
    return _frBuf[0];
  } else {
    return _frBuf[1];
  }
}

bool VGA::vsyncEvent(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *edata, void *user_ctx) {
    return true;
}

void VGA::swapBuffers() {
    if (xSemaphoreTakeFromISR(_sem_gui_ready, NULL) == pdTRUE) {
        if (_fbIndex == 0) {
            _fbIndex = 1;
        } else {
            _fbIndex = 0;
        }
        xSemaphoreGiveFromISR(_sem_vsync_end, NULL);
    }
}

bool VGA::bounceEvent(esp_lcd_panel_handle_t panel, void* bounce_buf, int pos_px, int len_bytes, void* user_ctx) {
    VGA* vga = (VGA*)user_ctx;
    uint16_t* bbuf = (uint16_t*)bounce_buf;
    int screenLineIndex = pos_px / vga->_Width;
    int bufLineIndex = screenLineIndex;
    uint16_t* pptr = vga->_frBuf[vga->_fbIndex] + (bufLineIndex*vga->_Width);
    int screenLines = len_bytes / vga->_Width;
    int lines = screenLines;
    uint16_t* bbptr = (uint16_t*)bbuf;
    int copyBytes = lines*vga->_Width;
    esp_async_memcpy(async_mem_handle,bbptr,pptr,copyBytes,NULL,NULL);
    if (pos_px >= vga->_lastBBuf) {
        vga->swapBuffers();
    }
    return true;
}

void VGA::dot(int x, int y, uint16_t color){
    uint16_t *ptr = (uint16_t *)getDrawBuffer();
    ptr+=x;
    ptr+=y*_Width;
    *ptr=color;
}

void VGA::clear()
  {
    uint16_t *ptr = (uint16_t *)getDrawBuffer();
    memset(ptr, 0,_Width* _Height*2);
}

void VGA::xLine(int x0, int x1, int y, uint16_t color)
  {
    if (y < 0 || y >= _Height)
      return;
    if (x0 > x1)
    {
      int xb = x0;
      x0 = x1;
      x1 = xb;
    }
    if (x0 < 0)
      x0 = 0;
    if (x1 > _Width)
      x1 = _Width;
    for (int x = x0; x < x1; x++) dot(x, y, color);
}

void VGA::triangle(short *v0, short *v1, short *v2, int color)
  {
    short *v[3] = {v0, v1, v2};
    if (v[1][1] < v[0][1])
    {
      short *vb = v[0];
      v[0] = v[1];
      v[1] = vb;
    }
    if (v[2][1] < v[1][1])
    {
      short *vb = v[1];
      v[1] = v[2];
      v[2] = vb;
    }
    if (v[1][1] < v[0][1])
    {
      short *vb = v[0];
      v[0] = v[1];
      v[1] = vb;
    }
    int y = v[0][1];
    int xac = v[0][0] << 16;
    int xab = v[0][0] << 16;
    int xbc = v[1][0] << 16;
    int xaci = 0;
    int xabi = 0;
    int xbci = 0;
    if (v[1][1] != v[0][1])
      xabi = ((v[1][0] - v[0][0]) << 16) / (v[1][1] - v[0][1]);
    if (v[2][1] != v[0][1])
      xaci = ((v[2][0] - v[0][0]) << 16) / (v[2][1] - v[0][1]);
    if (v[2][1] != v[1][1])
      xbci = ((v[2][0] - v[1][0]) << 16) / (v[2][1] - v[1][1]);

    for (; y < v[1][1] && y < _Height; y++)
    {
      if (y >= 0)
        xLine(xab >> 16, xac >> 16, y, color);
      xab += xabi;
      xac += xaci;
    }
    for (; y < v[2][1] && y < _Height; y++)
    {
      if (y >= 0)
        xLine(xbc >> 16, xac >> 16, y, color);
      xbc += xbci;
      xac += xaci;
    }
}

void VGA::line(int x1, int y1, int x2, int y2, uint16_t color)
  {
    int x, y, xe, ye;
    int dx = x2 - x1;
    int dy = y2 - y1;
    int dx1 = labs(dx);
    int dy1 = labs(dy);
    int px = 2 * dy1 - dx1;
    int py = 2 * dx1 - dy1;
    if (dy1 <= dx1)
    {
      if (dx >= 0)
      {
        x = x1;
        y = y1;
        xe = x2;
      }
      else
      {
        x = x2;
        y = y2;
        xe = x1;
      }
      dot(x, y, color);
      for (int i = 0; x < xe; i++)
      {
        x = x + 1;
        if (px < 0)
        {
          px = px + 2 * dy1;
        }
        else
        {
          if ((dx < 0 && dy < 0) || (dx > 0 && dy > 0))
          {
            y = y + 1;
          }
          else
          {
            y = y - 1;
          }
          px = px + 2 * (dy1 - dx1);
        }
        dot(x, y, color);
      }
    }
    else
    {
      if (dy >= 0)
      {
        x = x1;
        y = y1;
        ye = y2;
      }
      else
      {
        x = x2;
        y = y2;
        ye = y1;
      }

      dot(x, y, color);
      for (int i = 0; y < ye; i++)
      {
        y = y + 1;
        if (py <= 0)
        {
          py = py + 2 * dx1;
        }
        else
        {
          if ((dx < 0 && dy < 0) || (dx > 0 && dy > 0))
          {
            x = x + 1;
          }
          else
          {
            x = x - 1;
          }
          py = py + 2 * (dx1 - dy1);
        }
        dot(x, y, color);
      }
    }
  }

void VGA::fillRect(int x, int y, int w, int h, uint16_t color)
  {
    if (x < 0)
    {
      w += x;
      x = 0;
    }
    if (y < 0)
    {
      h += y;
      y = 0;
    }
    if (x + w > _Width)
      w = _Width - x;
    if (y + h > _Height)
      h = _Height - y;
      int i=0;
    for (int j = y; j < y + h; j++)
    {
      xLine(x,x+w,y+i,color);
      i++;
      }
  }

void VGA::rect(int x, int y, int w, int h, int color)
  {
    fillRect(x, y, w, 1, color);
    fillRect(x, y, 1, h, color);
    fillRect(x, y + h - 1, w, 1, color);
    fillRect(x + w - 1, y, 1, h, color);
  }

void VGA::circle(int x, int y, int r, int color)
  {
    int oxr = r;
    for(int i = 0; i < r + 1; i++)
    {
      int xr = (int)sqrtf(r * r - i * i);
      xLine(x - oxr, x - xr + 1, y + i, color);
      xLine(x + xr, x + oxr + 1, y + i, color);
      if(i) 
      {
        xLine(x - oxr, x - xr + 1, y - i, color);
        xLine(x + xr, x + oxr + 1, y - i, color);
      }
      oxr = xr;
    }
  }

void VGA::fillCircle(int x, int y, int r, int color)
  {
    for(int i = 0; i < r + 1; i++)
    {
      int xr = (int)sqrtf(r * r - i * i);
      xLine(x - xr, x + xr + 1, y + i, color);
      if(i) 
        xLine(x - xr, x + xr + 1, y - i, color);
    }
  }

void VGA::ellipse(int x, int y, int rx, int ry, int color)
  {
    if(ry == 0)
      return;
    int oxr = rx;
    float f = float(rx) / ry;
    f *= f;
    for(int i = 0; i < ry + 1; i++)
    {
      float s = rx * rx - i * i * f;
      int xr = (int)sqrtf(s <= 0 ? 0 : s);
      xLine(x - oxr, x - xr + 1, y + i, color);
      xLine(x + xr, x + oxr + 1, y + i, color);
      if(i) 
      {
        xLine(x - oxr, x - xr + 1, y - i, color);
        xLine(x + xr, x + oxr + 1, y - i, color);
      }
      oxr = xr;
    }
  }

void VGA::fillEllipse(int x, int y, int rx, int ry, int color)
  {
    if(ry == 0)
      return;
    float f = float(rx) / ry;
    f *= f;   
    for(int i = 0; i < ry + 1; i++)
    {
      float s = rx * rx - i * i * f;
      int xr = (int)sqrtf(s <= 0 ? 0 : s);
      xLine(x - xr, x + xr + 1, y + i, color);
      if(i) 
        xLine(x - xr, x + xr + 1, y - i, color);
    }
  }

void VGA::drawChar(int x, int y, int ch)
    {
        if (!font)
            return;
        if (!font->valid(ch))
            return;
        const unsigned char *pix = &font->pixels[font->charWidth * font->charHeight * (ch - font->firstChar)];

        for (int py = 0; py < font->charHeight; py++)
            for (int px = 0; px < font->charWidth; px++)
                if (*(pix++))
                    dot(px + x, py + y, frontColor);
                else
                    dot(px + x, py + y, backColor);
}

void VGA::setFont(Font &font)
    {
        this->font = &font;
    }

void VGA::setCursor(int x, int y)
    {   
        cursorX = cursorBaseX = x;
        cursorY = y;
    }
    
   
void VGA::setTextColor(int front, int back)
    {
        frontColor = front;
        backColor = back;
    }

void VGA::print(const char ch)
{
        if (!font)
            return;
        if (font->valid(ch))
            drawChar(cursorX, cursorY, ch);
        else
            drawChar(cursorX, cursorY, ' ');
        cursorX += font->charWidth;
        if (cursorX + font->charWidth > _Width)
        {
            cursorX = cursorBaseX;
            cursorY += font->charHeight;
            if(autoScroll && cursorY + font->charHeight > _Height)
               cursorY = font->charHeight;
        }
}
void VGA::println(const char ch)
    {
        print(ch);
        print("\n");
    }

void VGA::print(const char *str)
    {
        if (!font)
            return;
        while (*str)
        {
            if(*str == '\n')
            {
                cursorX = cursorBaseX;
                cursorY += font->charHeight;
            }
            else
                print(*str);
            str++;
        }
    }

void VGA::println(const char *str)
    {   
        print(str);
        print("\n");
    }
void VGA::print(long number, int base, int minCharacters)
    {
        if(minCharacters < 1)
            minCharacters = 1;
        bool sign = number < 0;
        if (sign)
            number = -number;
        const char baseChars[] = "0123456789ABCDEF";
        char temp[33];
        temp[32] = 0;
        int i = 31;
        do
        {
            temp[i--] = baseChars[number % base];
            number /= base;
        } while (number > 0);
        if (sign)
            temp[i--] = '-';
        for (; i > 31 - minCharacters; i--)
            temp[i] = ' ';
        print(&temp[i + 1]);
    }

void VGA::print(unsigned long number, int base, int minCharacters)
  {
    if(minCharacters < 1)
      minCharacters = 1;
    const char baseChars[] = "0123456789ABCDEF";
    char temp[33];
    temp[32] = 0;
    int i = 31;
    do
    {
      temp[i--] = baseChars[number % base];
      number /= base;
    } while (number > 0);
    for (; i > 31 - minCharacters; i--)
      temp[i] = ' ';
    print(&temp[i + 1]);
  } 

void VGA::println(long number, int base, int minCharacters)
  {
    print(number, base, minCharacters); print("\n");
  }

void VGA::println(unsigned long number, int base, int minCharacters)
  {
    print(number, base, minCharacters); print("\n");
  }

void VGA::print(int number, int base, int minCharacters)
  {
    print(long(number), base, minCharacters);
  }

void VGA::println(int number, int base, int minCharacters)
  {
    println(long(number), base, minCharacters);
  }

void VGA::print(unsigned int number, int base, int minCharacters)
  {
    print((unsigned long)(number), base, minCharacters);
  }

void VGA::println(unsigned int number, int base, int minCharacters)
  {
    println((unsigned long)(number), base, minCharacters);
  }

void VGA::print(short number, int base, int minCharacters)
  {
    print(long(number), base, minCharacters);
  }

void VGA::println(short number, int base, int minCharacters)
  {
    println(long(number), base, minCharacters);
  }

void VGA::print(unsigned short number, int base, int minCharacters)
  {
    print(long(number), base, minCharacters);
  }

void VGA::println(unsigned short number, int base, int minCharacters)
  {
    println(long(number), base, minCharacters);
  }

void VGA::print(unsigned char number, int base, int minCharacters)
  {
    print(long(number), base, minCharacters);
  }

void VGA::println(unsigned char number, int base, int minCharacters)
  {
    println(long(number), base, minCharacters);
  }

void VGA::println()
  {
    print("\n");
  }

void VGA::print(double number, int fractionalDigits, int minCharacters)
  {
    long p = long(pow(10, fractionalDigits));
    long long n = (double(number) * p + 0.5f);
    print(long(n / p), 10, minCharacters - 1 - fractionalDigits);
    if(fractionalDigits)
    {
      print(".");
      for(int i = 0; i < fractionalDigits; i++)
      {
        p /= 10;
        print(long(n / p) % 10);
      }
    }
  }

void VGA::println(double number, int fractionalDigits, int minCharacters)
  {
    print(number, fractionalDigits, minCharacters); 
    print("\n");
  }
