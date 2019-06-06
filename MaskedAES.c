

// This is a first-order protected byte-wise masked AES, using the masking scheme in the Chap 9 from the DPA Book (Mangard S, Oswald E, Popp T. Power analysis attacks: Revealing the secrets of smart cards. Springer Science &amp;Business Media, 2007)
//Following Dan's README.md on https://github.com/danpage/scale-hw/tree/fa15667bf32cdae6ed65c8f2c26735290ff85429
//For M0: export TARGET="${SCALE_HW}/target/lpc1114fn28"
//Make & Program: sudo  make --no-builtin-rules -f ${TARGET}/build/lib/scale.mk BSP="${TARGET}/build" USB="/dev/ttyUSB0" PROJECT="MaskedAES" PROJECT_SOURCES="MaskedAES.c MaskedAES.S" clean all program
//	  10 rounds' encryption (excluding the first addroundkey)     :12MHz <1.18ms
//	  Image Size					:3924
//For M3: export TARGET="${SCALE_HW}/target/lpc1313fbd48"
//Make & Program: sudo  make --no-builtin-rules -f ${TARGET}/build/lib/scale.mk BSP="${TARGET}/build" USB="/dev/ttyUSB0" PROJECT="MaskedAES" PROJECT_SOURCES="MaskedAES.c MaskedAES.S" clean all program
//	  10 rounds' encryption (excluding the first addroundkey)     :12MHz <1.19ms
//	  Image Size					:3508
#include <stdio.h>
#include <stdlib.h>

#include "MaskedAES.h"


void AES_encrypt(uint8_t* m, uint8_t* c)
{
   uint8_t temp[16];
  for(int j=0;j<16;j++)
  {
    m[j]=0;
    temp[j]=0;
  }

  MaskingKey(RoundKey, temp);
  MADK(c,temp);


  for(int i=0;i<10;i++)
  {
        MSbox(c);


        MShiftRow(c);

        


        if(i!=9)
        {

           MMixColumn(c, m);

           MaskingKey(RoundKey+(i+1)*16, temp);

           MADK(m,temp);

           SafeCopy(m,c);

        }
        else
        {
           MaskingKey(RoundKey+(i+1)*16, temp);
           MADK(c,temp);
           SafeCopy(c,m);
	  
        }

       
  }
  scale_gpio_wr( SCALE_GPIO_PIN_TRG, true  );//Trigger On
  scale_gpio_wr( SCALE_GPIO_PIN_TRG, false  );//Trigger Off

  Finalize(m, c);


  
}

int main( int argc, char* argv[] ) {

 if( !scale_init( &SCALE_CONF ) ) {
    return -1;
  }
  uint8_t plain[16];
  uint8_t cipher[16];
  uint8_t key[ 16 ] = { 0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
                      0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C };
    
    U = 0;
    V = 0;
    UV = 0;
   
    while( true ) 
    {

         for( int i = 0; i < 16; i++ ) 
	 {
             	plain[i]=(uint8_t)scale_uart_rd(SCALE_UART_MODE_BLOCKING);
         }
         U=(uint8_t)scale_uart_rd(SCALE_UART_MODE_BLOCKING);
	 V=(uint8_t)scale_uart_rd(SCALE_UART_MODE_BLOCKING);
         SRMask=(scale_uart_rd(SCALE_UART_MODE_BLOCKING)<<24)|(scale_uart_rd(SCALE_UART_MODE_BLOCKING)<<16)|(scale_uart_rd(SCALE_UART_MODE_BLOCKING)<<8)|(scale_uart_rd(SCALE_UART_MODE_BLOCKING));

         KeyExpansion(key);
         GenMaskedSbox();
         MaskingPlaintext(plain, cipher);
         AES_encrypt(plain, cipher);

    	 for( int i = 0; i <16; i++ ) 
	 {
     		scale_uart_wr(SCALE_UART_MODE_BLOCKING,( (char)cipher[ i ] ));
    	 }
    }


    return 0;
}
