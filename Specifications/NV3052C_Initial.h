
/******************************************************************************/
/* Panel Name : 3.5IPS                                  */
/* Resulation : 640x480                                                       */
/******************************************************************************/

/* LCD  panel SPI interface define */
sbit	SPI_DI = P1^3;	//data input		 
sbit	SPI_CLK = P1^7;   //clock
sbit	SPI_CS = P2^6;  //chip select
sbit	SPI_RES = P2^3;	 //reset



void SPI_SendData(unsigned char i)
{  
   unsigned char n;
   
   for(n=0; n<8; n++)			
   {  
	  if(i&0x80) SPI_DI=1;
      	else SPI_DI=0;
      i<<= 1;

	  SPI_CLK=0;//_nop_(); _nop_();_nop_();_nop_();
      SPI_CLK=1;//_nop_();_nop_();_nop_();_nop_();
   }
}
void SPI_WriteComm(unsigned char i)
{
    SPI_CS=0;

    SPI_DI=0;

	SPI_CLK=0;//_nop_(); _nop_();_nop_();_nop_();
	SPI_CLK=1;//_nop_();_nop_();_nop_();_nop_();

	SPI_SendData(i);

    SPI_CS=1;
}

void SPI_WriteData(unsigned char i)
{
    SPI_CS=0;

    SPI_DI=1;

	SPI_CLK=0;//_nop_(); _nop_();_nop_();_nop_();
	SPI_CLK=1;//_nop_();_nop_();_nop_();_nop_();

	SPI_SendData(i);

    SPI_CS=1;
}
	


void NV3052_Initial(void)
{   
	SPI_RES=1;
	delay(100);
	SPI_RES=0;
    delay(100);
    SPI_RES=1;
	delay(100);

	    	
 SPI_WriteComm(0xFF);SPI_WriteData(0x30);
SPI_WriteComm(0xFF);SPI_WriteData(0x52);
SPI_WriteComm(0xFF);SPI_WriteData(0x01);  
SPI_WriteComm(0xE3);SPI_WriteData(0x00);  
SPI_WriteComm(0x40);SPI_WriteData(0x00);
SPI_WriteComm(0x03);SPI_WriteData(0x40);
SPI_WriteComm(0x04);SPI_WriteData(0x00);
SPI_WriteComm(0x05);SPI_WriteData(0x03);
SPI_WriteComm(0x08);SPI_WriteData(0x00);
SPI_WriteComm(0x09);SPI_WriteData(0x07);
SPI_WriteComm(0x0A);SPI_WriteData(0x01);
SPI_WriteComm(0x0B);SPI_WriteData(0x32);
SPI_WriteComm(0x0C);SPI_WriteData(0x32);
SPI_WriteComm(0x0D);SPI_WriteData(0x0B);
SPI_WriteComm(0x0E);SPI_WriteData(0x00);
SPI_WriteComm(0x23);SPI_WriteData(0xA2);

SPI_WriteComm(0x24);SPI_WriteData(0x0c);
SPI_WriteComm(0x25);SPI_WriteData(0x06);
SPI_WriteComm(0x26);SPI_WriteData(0x14);
SPI_WriteComm(0x27);SPI_WriteData(0x14);

SPI_WriteComm(0x38);SPI_WriteData(0x9C); 
SPI_WriteComm(0x39);SPI_WriteData(0xA7); 
SPI_WriteComm(0x3A);SPI_WriteData(0x3a); 

SPI_WriteComm(0x28);SPI_WriteData(0x40);
SPI_WriteComm(0x29);SPI_WriteData(0x01);
SPI_WriteComm(0x2A);SPI_WriteData(0xdf);
SPI_WriteComm(0x49);SPI_WriteData(0x3C);   
SPI_WriteComm(0x91);SPI_WriteData(0x57); 
SPI_WriteComm(0x92);SPI_WriteData(0x57); 
SPI_WriteComm(0xA0);SPI_WriteData(0x55);
SPI_WriteComm(0xA1);SPI_WriteData(0x50);
SPI_WriteComm(0xA4);SPI_WriteData(0x9C);
SPI_WriteComm(0xA7);SPI_WriteData(0x02);  
SPI_WriteComm(0xA8);SPI_WriteData(0x01);  
SPI_WriteComm(0xA9);SPI_WriteData(0x01);  
SPI_WriteComm(0xAA);SPI_WriteData(0xFC);  
SPI_WriteComm(0xAB);SPI_WriteData(0x28);  
SPI_WriteComm(0xAC);SPI_WriteData(0x06);  
SPI_WriteComm(0xAD);SPI_WriteData(0x06);  
SPI_WriteComm(0xAE);SPI_WriteData(0x06);  
SPI_WriteComm(0xAF);SPI_WriteData(0x03);  
SPI_WriteComm(0xB0);SPI_WriteData(0x08);  
SPI_WriteComm(0xB1);SPI_WriteData(0x26);  
SPI_WriteComm(0xB2);SPI_WriteData(0x28);  
SPI_WriteComm(0xB3);SPI_WriteData(0x28);  
SPI_WriteComm(0xB4);SPI_WriteData(0x33);  
SPI_WriteComm(0xB5);SPI_WriteData(0x08);  
SPI_WriteComm(0xB6);SPI_WriteData(0x26);  
SPI_WriteComm(0xB7);SPI_WriteData(0x08);  
SPI_WriteComm(0xB8);SPI_WriteData(0x26); 
SPI_WriteComm(0xF0);SPI_WriteData(0x00); 
SPI_WriteComm(0xF6);SPI_WriteData(0xC0);


SPI_WriteComm(0xFF);SPI_WriteData(0x30);
SPI_WriteComm(0xFF);SPI_WriteData(0x52);
SPI_WriteComm(0xFF);SPI_WriteData(0x02);
SPI_WriteComm(0xB0);SPI_WriteData(0x0B);
SPI_WriteComm(0xB1);SPI_WriteData(0x16);
SPI_WriteComm(0xB2);SPI_WriteData(0x17); 
SPI_WriteComm(0xB3);SPI_WriteData(0x2C); 
SPI_WriteComm(0xB4);SPI_WriteData(0x32);  
SPI_WriteComm(0xB5);SPI_WriteData(0x3B);  
SPI_WriteComm(0xB6);SPI_WriteData(0x29); 
SPI_WriteComm(0xB7);SPI_WriteData(0x40);   
SPI_WriteComm(0xB8);SPI_WriteData(0x0d);
SPI_WriteComm(0xB9);SPI_WriteData(0x05);
SPI_WriteComm(0xBA);SPI_WriteData(0x12);
SPI_WriteComm(0xBB);SPI_WriteData(0x10);
SPI_WriteComm(0xBC);SPI_WriteData(0x12);
SPI_WriteComm(0xBD);SPI_WriteData(0x15);
SPI_WriteComm(0xBE);SPI_WriteData(0x19);              
SPI_WriteComm(0xBF);SPI_WriteData(0x0E);
SPI_WriteComm(0xC0);SPI_WriteData(0x16);  
SPI_WriteComm(0xC1);SPI_WriteData(0x0A);
SPI_WriteComm(0xD0);SPI_WriteData(0x0C);
SPI_WriteComm(0xD1);SPI_WriteData(0x17);
SPI_WriteComm(0xD2);SPI_WriteData(0x14);
SPI_WriteComm(0xD3);SPI_WriteData(0x2E);   
SPI_WriteComm(0xD4);SPI_WriteData(0x32);   
SPI_WriteComm(0xD5);SPI_WriteData(0x3C);  
SPI_WriteComm(0xD6);SPI_WriteData(0x22);
SPI_WriteComm(0xD7);SPI_WriteData(0x3D);
SPI_WriteComm(0xD8);SPI_WriteData(0x0D);
SPI_WriteComm(0xD9);SPI_WriteData(0x07);
SPI_WriteComm(0xDA);SPI_WriteData(0x13);
SPI_WriteComm(0xDB);SPI_WriteData(0x13);
SPI_WriteComm(0xDC);SPI_WriteData(0x11);
SPI_WriteComm(0xDD);SPI_WriteData(0x15);
SPI_WriteComm(0xDE);SPI_WriteData(0x19);                   
SPI_WriteComm(0xDF);SPI_WriteData(0x10);
SPI_WriteComm(0xE0);SPI_WriteData(0x17);    
SPI_WriteComm(0xE1);SPI_WriteData(0x0A);
SPI_WriteComm(0xFF);SPI_WriteData(0x30);
SPI_WriteComm(0xFF);SPI_WriteData(0x52);
SPI_WriteComm(0xFF);SPI_WriteData(0x03);   
SPI_WriteComm(0x00);SPI_WriteData(0x2A);
SPI_WriteComm(0x01);SPI_WriteData(0x2A);
SPI_WriteComm(0x02);SPI_WriteData(0x2A);
SPI_WriteComm(0x03);SPI_WriteData(0x2A);
SPI_WriteComm(0x04);SPI_WriteData(0x61);  
SPI_WriteComm(0x05);SPI_WriteData(0x80);   
SPI_WriteComm(0x06);SPI_WriteData(0xc7);   
SPI_WriteComm(0x07);SPI_WriteData(0x01);  
SPI_WriteComm(0x08);SPI_WriteData(0x03); 
SPI_WriteComm(0x09);SPI_WriteData(0x04);
SPI_WriteComm(0x70);SPI_WriteData(0x22);
SPI_WriteComm(0x71);SPI_WriteData(0x80);
SPI_WriteComm(0x30);SPI_WriteData(0x2A);
SPI_WriteComm(0x31);SPI_WriteData(0x2A);
SPI_WriteComm(0x32);SPI_WriteData(0x2A);
SPI_WriteComm(0x33);SPI_WriteData(0x2A);
SPI_WriteComm(0x34);SPI_WriteData(0x61);
SPI_WriteComm(0x35);SPI_WriteData(0xc5);
SPI_WriteComm(0x36);SPI_WriteData(0x80);
SPI_WriteComm(0x37);SPI_WriteData(0x23);
SPI_WriteComm(0x40);SPI_WriteData(0x03); 
SPI_WriteComm(0x41);SPI_WriteData(0x04); 
SPI_WriteComm(0x42);SPI_WriteData(0x05); 
SPI_WriteComm(0x43);SPI_WriteData(0x06); 
SPI_WriteComm(0x44);SPI_WriteData(0x11); 
SPI_WriteComm(0x45);SPI_WriteData(0xe8); 
SPI_WriteComm(0x46);SPI_WriteData(0xe9); 
SPI_WriteComm(0x47);SPI_WriteData(0x11);
SPI_WriteComm(0x48);SPI_WriteData(0xea); 
SPI_WriteComm(0x49);SPI_WriteData(0xeb);
SPI_WriteComm(0x50);SPI_WriteData(0x07); 
SPI_WriteComm(0x51);SPI_WriteData(0x08); 
SPI_WriteComm(0x52);SPI_WriteData(0x09); 
SPI_WriteComm(0x53);SPI_WriteData(0x0a); 
SPI_WriteComm(0x54);SPI_WriteData(0x11); 
SPI_WriteComm(0x55);SPI_WriteData(0xec); 
SPI_WriteComm(0x56);SPI_WriteData(0xed); 
SPI_WriteComm(0x57);SPI_WriteData(0x11); 
SPI_WriteComm(0x58);SPI_WriteData(0xef); 
SPI_WriteComm(0x59);SPI_WriteData(0xf0); 
SPI_WriteComm(0xB1);SPI_WriteData(0x01); 
SPI_WriteComm(0xB4);SPI_WriteData(0x15); 
SPI_WriteComm(0xB5);SPI_WriteData(0x16); 
SPI_WriteComm(0xB6);SPI_WriteData(0x09); 
SPI_WriteComm(0xB7);SPI_WriteData(0x0f); 
SPI_WriteComm(0xB8);SPI_WriteData(0x0d); 
SPI_WriteComm(0xB9);SPI_WriteData(0x0b); 
SPI_WriteComm(0xBA);SPI_WriteData(0x00); 
SPI_WriteComm(0xC7);SPI_WriteData(0x02); 
SPI_WriteComm(0xCA);SPI_WriteData(0x17); 
SPI_WriteComm(0xCB);SPI_WriteData(0x18); 
SPI_WriteComm(0xCC);SPI_WriteData(0x0a); 
SPI_WriteComm(0xCD);SPI_WriteData(0x10); 
SPI_WriteComm(0xCE);SPI_WriteData(0x0e); 
SPI_WriteComm(0xCF);SPI_WriteData(0x0c); 
SPI_WriteComm(0xD0);SPI_WriteData(0x00); 
SPI_WriteComm(0x81);SPI_WriteData(0x00); 
SPI_WriteComm(0x84);SPI_WriteData(0x15); 
SPI_WriteComm(0x85);SPI_WriteData(0x16); 
SPI_WriteComm(0x86);SPI_WriteData(0x10); 
SPI_WriteComm(0x87);SPI_WriteData(0x0a); 
SPI_WriteComm(0x88);SPI_WriteData(0x0c); 
SPI_WriteComm(0x89);SPI_WriteData(0x0e);
SPI_WriteComm(0x8A);SPI_WriteData(0x02); 
SPI_WriteComm(0x97);SPI_WriteData(0x00); 
SPI_WriteComm(0x9A);SPI_WriteData(0x17); 
SPI_WriteComm(0x9B);SPI_WriteData(0x18);
SPI_WriteComm(0x9C);SPI_WriteData(0x0f);
SPI_WriteComm(0x9D);SPI_WriteData(0x09); 
SPI_WriteComm(0x9E);SPI_WriteData(0x0b); 
SPI_WriteComm(0x9F);SPI_WriteData(0x0d); 
SPI_WriteComm(0xA0);SPI_WriteData(0x01); 
SPI_WriteComm(0xFF);SPI_WriteData(0x30);
SPI_WriteComm(0xFF);SPI_WriteData(0x52);
SPI_WriteComm(0xFF);SPI_WriteData(0x02);  
SPI_WriteComm(0x01);SPI_WriteData(0x01);
SPI_WriteComm(0x02);SPI_WriteData(0xDA);
SPI_WriteComm(0x03);SPI_WriteData(0xBA);
SPI_WriteComm(0x04);SPI_WriteData(0xA8);
SPI_WriteComm(0x05);SPI_WriteData(0x9A);
SPI_WriteComm(0x06);SPI_WriteData(0x70);
SPI_WriteComm(0x07);SPI_WriteData(0xFF);
SPI_WriteComm(0x08);SPI_WriteData(0x91);
SPI_WriteComm(0x09);SPI_WriteData(0x90);
SPI_WriteComm(0x0A);SPI_WriteData(0xFF);
SPI_WriteComm(0x0B);SPI_WriteData(0x8F);
SPI_WriteComm(0x0C);SPI_WriteData(0x60);
SPI_WriteComm(0x0D);SPI_WriteData(0x58);
SPI_WriteComm(0x0E);SPI_WriteData(0x48);
SPI_WriteComm(0x0F);SPI_WriteData(0x38);
SPI_WriteComm(0x10);SPI_WriteData(0x2B);
SPI_WriteComm(0xFF);SPI_WriteData(0x30);
SPI_WriteComm(0xFF);SPI_WriteData(0x52);
SPI_WriteComm(0xFF);SPI_WriteData(0x00);   
SPI_WriteComm(0x36);SPI_WriteData(0x02);
									  
SPI_WriteComm(0x11);SPI_WriteData(0x00);	 //sleep out
delay( 200 );

SPI_WriteComm(0x29);SPI_WriteData(0x00);	  //display on
delay(10);  



}


