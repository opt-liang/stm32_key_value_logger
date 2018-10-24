#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "key_value.h"
#include "spi_flash.h"
#include "elog.h"

#define ELOG_START_ADDR                     (0)
#define ELOG_AREA_SIZE                      (4*1024*200)
#define ELOG_ERASE_MIN_SIZE                 (4*1024)
#define ELOG_APP_INIT_MARK                  (0x4546)
#define ELOG_FULL_MARK                      (0x100)

static uint8_t ELOG_BUF[ELOG_ERASE_MIN_SIZE] = { 0 };
static uint32_t ELOG_CURR_WRITE_ADDR = ELOG_START_ADDR;
static bool ELOG_FULL_MARKS = false;

void elog_clean( void );

static void elog_get_full_mark( void ){
    uint32_t elog_full_mark = 0x00;
    if( !get_key_value( "ELOG_FULL_MARK", UINT32, (uint8_t *)(&elog_full_mark)) ){
        ELOG_ASSERT( false );
    }
    if( elog_full_mark == ELOG_FULL_MARK ){
        ELOG_FULL_MARKS = true;
    }
}

void elog_port_erase(uint32_t ADDR, uint32_t LEN) {
    
    ELOG_ASSERT( (ADDR % ELOG_ERASE_MIN_SIZE) == 0x00 );
    volatile uint16_t SECTOR = ADDR / ELOG_ERASE_MIN_SIZE + LEN / ELOG_ERASE_MIN_SIZE + (( LEN % ELOG_ERASE_MIN_SIZE ) ? 1 : 0 );
    for( uint16_t i = ADDR / ELOG_ERASE_MIN_SIZE; i <= SECTOR; i ++ ){
        FLASH_Erase_Sector( i );
    }
}

void elog_app_init( void ){
    uint32_t elog_app_init_mark = 0;
    if( !get_key_value( "ELOG_APP_INIT_MARK", UINT32, (uint8_t *)(&elog_app_init_mark)) || elog_app_init_mark != ELOG_APP_INIT_MARK ){
        elog_clean();
        elog_app_init_mark = ELOG_APP_INIT_MARK;
        if( !set_key_value( "ELOG_APP_INIT_MARK", UINT32, (uint8_t *)(&elog_app_init_mark) ) ){
            ELOG_ASSERT( false );
        }
        if( !set_key_value( "ELOG_CURR_WRITE_ADDR", UINT32, (uint8_t *)(&ELOG_CURR_WRITE_ADDR)) ){
            ELOG_ASSERT( false );
        }
        uint32_t elog_full_mark = 0x00;
        if( !set_key_value( "ELOG_FULL_MARK", UINT32, (uint8_t *)(&elog_full_mark)) ){
            ELOG_ASSERT( false );
        }
    }
    
    if( !get_key_value( "ELOG_CURR_WRITE_ADDR", UINT32, (uint8_t *)(&ELOG_CURR_WRITE_ADDR)) ){
        ELOG_ASSERT( false );
    }
    
    elog_get_full_mark();
}

void elog_show( void ){
	
    if( !get_key_value( "ELOG_CURR_WRITE_ADDR", UINT32, (uint8_t *)(&ELOG_CURR_WRITE_ADDR)) ){
        ELOG_ASSERT( false );
    }
    
    elog_get_full_mark();
    
    uint32_t CURR_ADDR = 0x00;
	bool CURR_MARK = (ELOG_CURR_WRITE_ADDR % ELOG_ERASE_MIN_SIZE == 0x00);
	uint32_t CURR_SECTOR_ADDR = ELOG_CURR_WRITE_ADDR / ELOG_ERASE_MIN_SIZE * ELOG_ERASE_MIN_SIZE;

	if( ELOG_FULL_MARKS ){
		for( CURR_ADDR = CURR_MARK ? CURR_SECTOR_ADDR : CURR_SECTOR_ADDR + ELOG_ERASE_MIN_SIZE; CURR_ADDR < (ELOG_START_ADDR + ELOG_AREA_SIZE); CURR_ADDR += ELOG_ERASE_MIN_SIZE ){
			FLASH_Read( ELOG_BUF, CURR_ADDR, ELOG_ERASE_MIN_SIZE );
			printf("%.*s", ELOG_ERASE_MIN_SIZE, ELOG_BUF );
		}
	}

    for( CURR_ADDR = ELOG_START_ADDR; CURR_ADDR < CURR_SECTOR_ADDR; CURR_ADDR += ELOG_ERASE_MIN_SIZE ){
        FLASH_Read( ELOG_BUF, CURR_ADDR, ELOG_ERASE_MIN_SIZE );
        printf("%.*s", ELOG_ERASE_MIN_SIZE, ELOG_BUF );
    }
	
    if( !CURR_MARK ){
        FLASH_Read( ELOG_BUF, CURR_ADDR, ELOG_CURR_WRITE_ADDR % ELOG_ERASE_MIN_SIZE );
        printf( "%.*s", ELOG_CURR_WRITE_ADDR % ELOG_ERASE_MIN_SIZE, ELOG_BUF );
    }
}

void elog_write( const char* LOG, uint16_t LEN ){

    if( (ELOG_CURR_WRITE_ADDR + LEN) > ( ELOG_START_ADDR + ELOG_AREA_SIZE) ){
        
        uint16_t LEFT_RAM = ELOG_START_ADDR + ELOG_AREA_SIZE - ELOG_CURR_WRITE_ADDR;
        if( LEFT_RAM ){
			FLASH_Write( (uint8_t *)LOG, ELOG_CURR_WRITE_ADDR, LEFT_RAM );
		}
		
        ELOG_CURR_WRITE_ADDR = ELOG_START_ADDR + (LEN - LEFT_RAM);
        if( !set_key_value( "ELOG_CURR_WRITE_ADDR", UINT32, (uint8_t *)(&ELOG_CURR_WRITE_ADDR)) ){
            ELOG_ASSERT( false );
        }
        
        if( !ELOG_FULL_MARKS ){
            uint32_t elog_full_mark = ELOG_FULL_MARK;
            if( !set_key_value( "ELOG_FULL_MARK", UINT32, (uint8_t *)(&elog_full_mark)) ){
                ELOG_ASSERT( false );
            }
            ELOG_FULL_MARKS = true;
        }
        
		FLASH_Erase_Sector( ELOG_START_ADDR / ELOG_ERASE_MIN_SIZE  );
		FLASH_Write( (uint8_t *)(LOG+LEFT_RAM), ELOG_START_ADDR, LEN-LEFT_RAM );
        
		return;
    }
    
    if( ELOG_FULL_MARKS ){
		if( ((ELOG_CURR_WRITE_ADDR+LEN) / ELOG_ERASE_MIN_SIZE) > (ELOG_CURR_WRITE_ADDR / ELOG_ERASE_MIN_SIZE) ){
			FLASH_Erase_Sector( (ELOG_CURR_WRITE_ADDR+LEN) / ELOG_ERASE_MIN_SIZE  );
		}
    }
    
    FLASH_Write( (uint8_t *)LOG, ELOG_CURR_WRITE_ADDR, LEN );
    
    ELOG_CURR_WRITE_ADDR += LEN;
    if( !set_key_value( "ELOG_CURR_WRITE_ADDR", UINT32, (uint8_t *)(&ELOG_CURR_WRITE_ADDR)) ){
        ELOG_ASSERT( false );
    }
}

void elog_clean( void ){
    elog_port_erase( ELOG_START_ADDR, ELOG_AREA_SIZE );
}

void elog_buff_flush( void ){
    elog_flush();
}

