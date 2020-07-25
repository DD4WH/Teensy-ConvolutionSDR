


void Menu_2_Assistant_Func (void)
  {
    #define xMenuAssis 10
    #define yMenuAssis 115

    int color1 = ILI9341_RED;
    int color2 = ILI9341_GREEN;

    int color = color2;

    if (Menu_2_Enc_Sub == 1)
       color = color1;
    else
       color = color2;
    
    tft.fillRect(0, 110, 265, 150, ILI9341_BLACK);

    tft.setCursor(xMenuAssis, yMenuAssis);
     

    if((Menu2 > last_menu+3)&&(Menu2 <= last_menu2-3))
      {
      tft.setTextColor(ILI9341_WHITE);
      tft.setFont(Arial_12);
      tft.print(Menus[Menu2-3].no);
      tft.print(". ");
      tft.print(Menus[Menu2-3].text1);
      tft.print(Menus[Menu2-3].text2);
    
      tft.setCursor(xMenuAssis, yMenuAssis+15);
      tft.setFont(Arial_12);
      tft.print(Menus[Menu2-2].no);
      tft.print(". ");
      tft.print(Menus[Menu2-2].text1);
      tft.print(Menus[Menu2-2].text2);

      tft.setCursor(xMenuAssis, yMenuAssis+30);
      tft.print(Menus[Menu2-1].no);
      tft.print(". ");
      tft.print(Menus[Menu2-1].text1);
      tft.print(Menus[Menu2-1].text2);

      tft.setTextColor(color);
      tft.setCursor(xMenuAssis, yMenuAssis+45);
      tft.print(Menus[Menu2].no);
      tft.print(". ");
      tft.print(Menus[Menu2].text1);
      tft.print(Menus[Menu2].text2);

      tft.setTextColor(ILI9341_WHITE);
      tft.setCursor(xMenuAssis, yMenuAssis+60);
      tft.print(Menus[Menu2+1].no);
      tft.print(". ");
      tft.print(Menus[Menu2+1].text1);
      tft.print(Menus[Menu2+1].text2);
    
      tft.setCursor(xMenuAssis, yMenuAssis+75);
      tft.print(Menus[Menu2+2].no);
      tft.print(". ");
      tft.print(Menus[Menu2+2].text1);
      tft.print(Menus[Menu2+2].text2);

      tft.setCursor(xMenuAssis, yMenuAssis+90);
      tft.print(Menus[Menu2+3].no);
      tft.print(". ");
      tft.print(Menus[Menu2+3].text1);
      tft.print(Menus[Menu2+3].text2);
      }
    else if (Menu2 <= last_menu+3)
      {
      tft.setTextColor(ILI9341_WHITE);
      
      if (Menu2 == last_menu+1)
        tft.setTextColor(color);
      else
        tft.setTextColor(ILI9341_WHITE);
      
      tft.setFont(Arial_12);
      tft.print(Menus[last_menu+1].no);
      tft.print(". ");
      tft.print(Menus[last_menu+1].text1);
      tft.print(Menus[last_menu+1].text2);

      if (Menu2 == last_menu+2)
        tft.setTextColor(color);
      else
        tft.setTextColor(ILI9341_WHITE);
    
      tft.setCursor(xMenuAssis, yMenuAssis+15);
      tft.setFont(Arial_12);
      tft.print(Menus[last_menu+2].no);
      tft.print(". ");
      tft.print(Menus[last_menu+2].text1);
      tft.print(Menus[last_menu+2].text2);

      if (Menu2 == last_menu+3)
        tft.setTextColor(color);
      else
        tft.setTextColor(ILI9341_WHITE);

      tft.setCursor(xMenuAssis, yMenuAssis+30);
      tft.print(Menus[last_menu+3].no);
      tft.print(". ");
      tft.print(Menus[last_menu+3].text1);
      tft.print(Menus[last_menu+3].text2);
      
      tft.setTextColor(ILI9341_WHITE);
      tft.setCursor(xMenuAssis, yMenuAssis+45);
      tft.print(Menus[last_menu+4].no);
      tft.print(". ");
      tft.print(Menus[last_menu+4].text1);
      tft.print(Menus[last_menu+4].text2);

      tft.setCursor(xMenuAssis, yMenuAssis+60);
      tft.print(Menus[last_menu+5].no);
      tft.print(". ");
      tft.print(Menus[last_menu+5].text1);
      tft.print(Menus[last_menu+5].text2);
    
      tft.setCursor(xMenuAssis, yMenuAssis+75);
      tft.print(Menus[last_menu+6].no);
      tft.print(". ");
      tft.print(Menus[last_menu+6].text1);
      tft.print(Menus[last_menu+6].text2);

      tft.setCursor(xMenuAssis, yMenuAssis+90);
      tft.print(Menus[last_menu+7].no);
      tft.print(". ");
      tft.print(Menus[last_menu+7].text1);
      tft.print(Menus[last_menu+7].text2); 
      }
    else if (Menu2 > last_menu2-3)
      {
      tft.setTextColor(ILI9341_WHITE);
      tft.setFont(Arial_12);
      tft.print(Menus[last_menu2-6].no);
      tft.print(". ");
      tft.print(Menus[last_menu2-6].text1);
      tft.print(Menus[last_menu2-6].text2);
    
      tft.setCursor(xMenuAssis, yMenuAssis+15);
      tft.setFont(Arial_12);
      tft.print(Menus[last_menu2-5].no);
      tft.print(". ");
      tft.print(Menus[last_menu2-5].text1);
      tft.print(Menus[last_menu2-5].text2);

      tft.setCursor(xMenuAssis, yMenuAssis+30);
      tft.print(Menus[last_menu2-4].no);
      tft.print(". ");
      tft.print(Menus[last_menu2-4].text1);
      tft.print(Menus[last_menu2-4].text2);
      
      tft.setCursor(xMenuAssis, yMenuAssis+45);
      tft.print(Menus[last_menu2-3].no);
      tft.print(". ");
      tft.print(Menus[last_menu2-3].text1);
      tft.print(Menus[last_menu2-3].text2);

      if (Menu2 == last_menu2-2)
        tft.setTextColor(color);
      else
        tft.setTextColor(ILI9341_WHITE);
      tft.setCursor(xMenuAssis, yMenuAssis+60);
      tft.print(Menus[last_menu2-2].no);
      tft.print(". ");
      tft.print(Menus[last_menu2-2].text1);
      tft.print(Menus[last_menu2-2].text2);

       if (Menu2 == last_menu2-1)
        tft.setTextColor(color);
      else
        tft.setTextColor(ILI9341_WHITE);
      tft.setCursor(xMenuAssis, yMenuAssis+75);
      tft.print(Menus[last_menu2-1].no);
      tft.print(". ");
      tft.print(Menus[last_menu2-1].text1);
      tft.print(Menus[last_menu2-1].text2);

       if (Menu2 == last_menu2)
        tft.setTextColor(color);
      else
        tft.setTextColor(ILI9341_WHITE);
      tft.setCursor(xMenuAssis, yMenuAssis+90);
      tft.print(Menus[last_menu2].no);
      tft.print(". ");
      tft.print(Menus[last_menu2].text1);
      tft.print(Menus[last_menu2].text2);
      }   

      DrawBlockDiagram();
    }



void Menu_1_Assistant_Func (void)
  {
    #define xMenuAssis 10
    #define yMenuAssis 115

    int color1 = ILI9341_RED;
    int color2 = ILI9341_GREEN;

    int color = color2;

    if (Menu_1_Enc_Sub == 1)
       color = color1;
    else
       color = color2;

    tft.fillRect(0, 110, 265, 150, ILI9341_BLACK);

    tft.setCursor(xMenuAssis, yMenuAssis);

    if((Menu_pointer > 2)&&(Menu_pointer <= last_menu-3))
      {
      tft.setTextColor(ILI9341_WHITE);
      tft.setFont(Arial_12);
      tft.print(Menus[Menu_pointer-3].no);
      tft.print(". ");
      tft.print(Menus[Menu_pointer-3].text1);
      tft.print(Menus[Menu_pointer-3].text2);
    
      tft.setCursor(xMenuAssis, yMenuAssis+15);
      tft.setFont(Arial_12);
      tft.print(Menus[Menu_pointer-2].no);
      tft.print(". ");
      tft.print(Menus[Menu_pointer-2].text1);
      tft.print(Menus[Menu_pointer-2].text2);

      tft.setCursor(xMenuAssis, yMenuAssis+30);
      tft.print(Menus[Menu_pointer-1].no);
      tft.print(". ");
      tft.print(Menus[Menu_pointer-1].text1);
      tft.print(Menus[Menu_pointer-1].text2);

      tft.setTextColor(color);
      tft.setCursor(xMenuAssis, yMenuAssis+45);
      tft.print(Menus[Menu_pointer].no);
      tft.print(". ");
      tft.print(Menus[Menu_pointer].text1);
      tft.print(Menus[Menu_pointer].text2);

      tft.setTextColor(ILI9341_WHITE);
      tft.setCursor(xMenuAssis, yMenuAssis+60);
      tft.print(Menus[Menu_pointer+1].no);
      tft.print(". ");
      tft.print(Menus[Menu_pointer+1].text1);
      tft.print(Menus[Menu_pointer+1].text2);
    
      tft.setCursor(xMenuAssis, yMenuAssis+75);
      tft.print(Menus[Menu_pointer+2].no);
      tft.print(". ");
      tft.print(Menus[Menu_pointer+2].text1);
      tft.print(Menus[Menu_pointer+2].text2);

      tft.setCursor(xMenuAssis, yMenuAssis+90);
      tft.print(Menus[Menu_pointer+3].no);
      tft.print(". ");
      tft.print(Menus[Menu_pointer+3].text1);
      tft.print(Menus[Menu_pointer+3].text2);
      }
    else if(Menu_pointer <= 2)
      {
      tft.setTextColor(ILI9341_WHITE);
      
      if (Menu_pointer == 0)
        tft.setTextColor(color);
      else
        tft.setTextColor(ILI9341_WHITE);
      
      tft.setFont(Arial_12);
      tft.print(Menus[0].no);
      tft.print(". ");
      tft.print(Menus[0].text1);
      tft.print(Menus[0].text2);

      if (Menu_pointer == 1)
        tft.setTextColor(color);
      else
        tft.setTextColor(ILI9341_WHITE);
    
      tft.setCursor(xMenuAssis, yMenuAssis+15);
      tft.setFont(Arial_12);
      tft.print(Menus[1].no);
      tft.print(". ");
      tft.print(Menus[1].text1);
      tft.print(Menus[1].text2);

      if (Menu_pointer == 2)
        tft.setTextColor(color);
      else
        tft.setTextColor(ILI9341_WHITE);

      tft.setCursor(xMenuAssis, yMenuAssis+30);
      tft.print(Menus[2].no);
      tft.print(". ");
      tft.print(Menus[2].text1);
      tft.print(Menus[2].text2);
      
      tft.setTextColor(ILI9341_WHITE);
      tft.setCursor(xMenuAssis, yMenuAssis+45);
      tft.print(Menus[3].no);
      tft.print(". ");
      tft.print(Menus[3].text1);
      tft.print(Menus[3].text2);

      tft.setCursor(xMenuAssis, yMenuAssis+60);
      tft.print(Menus[4].no);
      tft.print(". ");
      tft.print(Menus[4].text1);
      tft.print(Menus[4].text2);
    
      tft.setCursor(xMenuAssis, yMenuAssis+75);
      tft.print(Menus[5].no);
      tft.print(". ");
      tft.print(Menus[5].text1);
      tft.print(Menus[5].text2);

      tft.setCursor(xMenuAssis, yMenuAssis+90);
      tft.print(Menus[6].no);
      tft.print(". ");
      tft.print(Menus[6].text1);
      tft.print(Menus[6].text2);   
      }

   else if (Menu_pointer > last_menu-3)
      {
      tft.setTextColor(ILI9341_WHITE);
      tft.setFont(Arial_12);
      tft.print(Menus[last_menu-6].no);
      tft.print(". ");
      tft.print(Menus[last_menu-6].text1);
      tft.print(Menus[last_menu-6].text2);
    
      tft.setCursor(xMenuAssis, yMenuAssis+15);
      tft.setFont(Arial_12);
      tft.print(Menus[last_menu-5].no);
      tft.print(". ");
      tft.print(Menus[last_menu-5].text1);
      tft.print(Menus[last_menu-5].text2);

      tft.setCursor(xMenuAssis, yMenuAssis+30);
      tft.print(Menus[last_menu-4].no);
      tft.print(". ");
      tft.print(Menus[last_menu-4].text1);
      tft.print(Menus[last_menu-4].text2);
      
      tft.setCursor(xMenuAssis, yMenuAssis+45);
      tft.print(Menus[last_menu-3].no);
      tft.print(". ");
      tft.print(Menus[last_menu-3].text1);
      tft.print(Menus[last_menu-3].text2);

      if (Menu_pointer == last_menu-2)
        tft.setTextColor(color);
      else
        tft.setTextColor(ILI9341_WHITE);
      tft.setCursor(xMenuAssis, yMenuAssis+60);
      tft.print(Menus[last_menu-2].no);
      tft.print(". ");
      tft.print(Menus[last_menu-2].text1);
      tft.print(Menus[last_menu-2].text2);

       if (Menu_pointer == last_menu-1)
        tft.setTextColor(color);
      else
        tft.setTextColor(ILI9341_WHITE);
      tft.setCursor(xMenuAssis, yMenuAssis+75);
      tft.print(Menus[last_menu-1].no);
      tft.print(". ");
      tft.print(Menus[last_menu-1].text1);
      tft.print(Menus[last_menu-1].text2);

       if (Menu_pointer == last_menu)
        tft.setTextColor(color);
      else
        tft.setTextColor(ILI9341_WHITE);
      tft.setCursor(xMenuAssis, yMenuAssis+90);
      tft.print(Menus[last_menu].no);
      tft.print(". ");
      tft.print(Menus[last_menu].text1);
      tft.print(Menus[last_menu].text2);
      }

      DrawBlockDiagram();  
  } 



void DrawBlockDiagram (void)
  {

    #define xBlockDiagram 165         //Location of the block diagram
    #define yBlockDiagram 125

    #define xBlockSize 80
    #define yBlockSize 35
    

    int color0 = ILI9341_WHITE;
    int color1 = ILI9341_RED;
    int color2 = ILI9341_GREEN;

    int color_T0 = color0;
    int color_T1 = color0;
   
  
  //Draw the blocks
  tft.drawRect(xBlockDiagram , yBlockDiagram, xBlockSize, yBlockSize, color0);
  tft.drawRect(xBlockDiagram, yBlockDiagram + yBlockSize +5, xBlockSize, yBlockSize, color0);
  tft.drawRect(xBlockDiagram , yBlockDiagram +2*yBlockSize +10 , xBlockSize, yBlockSize, color0);

  tft.drawLine(xBlockDiagram + xBlockSize/2, yBlockDiagram + yBlockSize, xBlockDiagram + xBlockSize/2, yBlockDiagram + yBlockSize + 5 , color0);
  tft.drawLine(xBlockDiagram + xBlockSize/2, yBlockDiagram + 2*yBlockSize + 5, xBlockDiagram + xBlockSize/2, yBlockDiagram + 2*yBlockSize + 10 , color0);
  
  if((Menu_2_Assistant == 1)&&((Menu2==21)||(Menu2==22)||(Menu2 >= 65)))         //Menu assitant is the section with the MSI001 chip
    {
    //Draw the antennas
    tft.drawLine(xBlockDiagram + 20, yBlockDiagram, xBlockDiagram + 20, yBlockDiagram - 10 , color0);
    tft.drawLine(xBlockDiagram + 15, yBlockDiagram-10, xBlockDiagram + 20, yBlockDiagram - 4 , color0);
    tft.drawLine(xBlockDiagram + 25, yBlockDiagram-10, xBlockDiagram + 20, yBlockDiagram - 4 , color0);
  
    tft.drawLine(xBlockDiagram + 60, yBlockDiagram, xBlockDiagram + 60, yBlockDiagram - 10 , color0);
    tft.drawLine(xBlockDiagram + 55, yBlockDiagram-10, xBlockDiagram + 60, yBlockDiagram - 4 , color0);
    tft.drawLine(xBlockDiagram + 65, yBlockDiagram-10, xBlockDiagram + 60, yBlockDiagram - 4 , color0);

  
    //Draw the text in the blocks
    tft.setFont(Arial_8);
    if(Menu2==MENU_ANT_BIAST)
      {
      if(ANT_BIAST == 0)
        {
          color_T0 = color2;
          color_T1 = color0;
        }
      else
        {
          color_T0 = color0;
          color_T1 = color2;
        }
      }
     else
      {
        color_T0 = color0;
        color_T1 = color0;
      } 

    
    tft.setTextColor(color_T0);
    tft.setCursor(xBlockDiagram + 5, yBlockDiagram + 3);
    tft.print("ANT1");

    tft.setTextColor(color_T1);    
    tft.setCursor(xBlockDiagram + 48, yBlockDiagram + 3);
    tft.print("ANT2");    
          
    tft.setTextColor(color0);
    tft.setCursor(xBlockDiagram + 15, yBlockDiagram + 14);
    tft.print("BPF Board");

    if(Menu2==MENU_RF_BPF)
      color_T0 = color2;
    else
      color_T0 = color0;  
    tft.setTextColor(color_T0);
    tft.setCursor(xBlockDiagram + 5, yBlockDiagram + 25);
    tft.print("RF. BPF");

  
    //Second block
    if(Menu2==MENU_RF_Preamp)
      color_T0 = color2;
    else
      color_T0 = color0;  
    tft.setTextColor(color_T0);
    tft.setCursor(xBlockDiagram + 5, yBlockDiagram + yBlockSize + 5 + 3);
    tft.print("RF. Preamp");

    if(Menu2==MENU_RF_ATTENUATION)
      color_T0 = color2;
    else
      color_T0 = color0;  
    tft.setTextColor(color_T0);
    tft.setCursor(xBlockDiagram + 5, yBlockDiagram + yBlockSize + 5 + 14);
    tft.print("RF. Atten");

    if(Menu2==MENU_RF_Range)
      color_T0 = color2;
    else
      color_T0 = color0;  
    tft.setTextColor(color_T0);
    tft.setCursor(xBlockDiagram + 5, yBlockDiagram + yBlockSize + 5 + 25);
    tft.print("RF. Range");

    tft.setTextColor(color0);
    tft.setCursor(xBlockDiagram + 60, yBlockDiagram + yBlockSize + 5 + 14);
    tft.print("MSi");
    tft.setCursor(xBlockDiagram + 60, yBlockDiagram + yBlockSize + 5 + 25);
    tft.print("001");

    //Third block
    tft.setCursor(xBlockDiagram + 30, yBlockDiagram + 2*yBlockSize + 10 + 14);
    tft.print("ADC");

    if(Menu2==MENU_RF_GAIN)
      color_T0 = color2;
    else
      color_T0 = color0;  
    tft.setTextColor(color_T0);
    tft.setCursor(xBlockDiagram + 5, yBlockDiagram + 2*yBlockSize + 10 + 25);
    tft.print("RF. Gain");

    tft.setTextColor(color0);
    tft.setCursor(xBlockDiagram - 95, yBlockDiagram + 2*yBlockSize + 10 + 25);
    tft.print("Block Diagram Pt.1");
    }

  else  
    {
    //Draw the line on the top (continue from the top page)
    tft.drawLine(xBlockDiagram + xBlockSize/2, yBlockDiagram, xBlockDiagram + xBlockSize/2, yBlockDiagram - 5 , color0);

    //Draw the text in the blocks
    tft.setFont(Arial_8);  
      
    if(((Menu2>MENU_RF_ATTENUATION)&&(Menu2<MENU_SPK_EN))||(Menu_1_Assistant == 1))
      color_T0 = color2;
    else
      color_T0 = color0;  
    tft.setTextColor(color_T0);
    tft.setCursor(xBlockDiagram + 10, yBlockDiagram + 14);
    tft.print("Teensy 4.0");
  
    //Second block

    if((Menu2==MENU_VOLUME)&&(Menu_2_Assistant == 1))
      color_T0 = color2;
    else
      color_T0 = color0;  
    tft.setTextColor(color_T0);
    tft.setCursor(xBlockDiagram + 30, yBlockDiagram + yBlockSize + 5 + 14);
    tft.print("DAC");

    //Third block
    tft.setTextColor(color0);
    tft.setCursor(xBlockDiagram + 10, yBlockDiagram + 2*yBlockSize + 10 + 14);
    tft.print("Audio Amp.");

    if((Menu2==MENU_SPK_EN)&&(Menu_2_Assistant == 1))
      color_T0 = color2;
    else
      color_T0 = color0;  
    tft.setTextColor(color_T0);
    tft.setCursor(xBlockDiagram + 5, yBlockDiagram + 2*yBlockSize + 10 + 25);
    tft.print("EN. SPK."); 
    
    tft.setTextColor(color0);
    tft.setCursor(xBlockDiagram - 95, yBlockDiagram + 2*yBlockSize + 10 + 25);
    tft.print("Block Diagram Pt.2");
    }
  }
     
