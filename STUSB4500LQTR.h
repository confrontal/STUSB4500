#ifndef STUSB4500LQTR_H
#define STUSB4500LQTR_H

//https://github.com/usb-c/STUSB4500/blob/master/Firmware/Project/Src/USB_PD_core.c

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "STUSB4500_register_map.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include <string.h>

typedef struct STUSB4500_s{
    bool initialized;
    uint8_t address;
    int i2c_master_num;
    TickType_t ticks_to_wait;
    uint8_t sector[5][8];
    uint8_t readSectors;
}STUSB4500_s;

typedef enum STUSB4500_e{
  
  ALERT_STATUS_1 = 0x0B,
  ALERT_STATUS_1_MASK = 0x0C,
  PORT_STATUS_0  = 0x0D,
  PORT_STATUS_1  = 0x0E,
  TYPEC_MONITORING_STATUS_0 = 0x0F,
  TYPEC_MONITORING_STATUS_1  = 0x10,
  CC_STATUS = 0x11,
  CC_HW_FAULT_STATUS_0  = 0x12,
  CC_HW_FAULT_STATUS_1  = 0x13,
  PD_TYPEC_STATUS  = 0x14,
  TYPEC_STATUS = 0x15,
  PRT_STATUS = 0x16,

  RDO_REG_STATUS_0 = 0x91,
  RDO_REG_STATUS_1 = 0x92,
  RDO_REG_STATUS_2 = 0x93,
  RDO_REG_STATUS_3 = 0x94,
}STUSB4500_e;

typedef union ALERT_STATUS_s{
  uint8_t d8;
  struct {
    uint8_t reserved_7                  : 1;
    uint8_t port_Status_al              : 1;
    uint8_t typec_monitoring_status_al  : 1;
    uint8_t cc_hw_fault_status_al       : 1;
    uint8_t reserved_2                  : 2;
    uint8_t prt_Status_al               : 1;
    uint8_t reserved_0                  : 1;
  }d;
}ALERT_STATUS_s;

typedef union PORT_STATUS_s{
  uint8_t d8;
  struct{
    uint8_t attached_device   : 3;
    uint8_t reserved_4        : 1;
    uint8_t power_mode        : 1;
    uint8_t data_mode         : 1;
    uint8_t reserved_1        : 1;
    uint8_t attach            : 1;
  }d;
}PORT_STATUS_s;

typedef union RDO_REG_s
{
  uint32_t d32;
  struct
  {
    uint8_t reserved_31                 : 1;
    uint8_t obj_pos                     : 3;
    uint8_t give_back_flag              : 1;
    uint8_t capability_mismatch         : 1;
    uint8_t usb_comm_capable            : 1;
    uint8_t no_usb_suspend              : 1;
    uint8_t unchunked_ext_msg_supported : 1;
    uint8_t reserved_20                 : 3;
    uint16_t operating_current          : 10;
    uint16_t max_operating_current      : 10;
  }d;
}RDO_REG_s;

    /*
      Initializes the I2C bus. If the device ID is configured for a address other than the default
      	it should be intialized here. Valid IDs are 0x28, 0x29, 0x2A, and 0x2B. If another
        I2C bus is used (such as 1 or 2), it can be defined here as well.
    */
    esp_err_t STUSB_begin(STUSB4500_s *device, uint8_t deviceAddress, int i2c_master_num, TickType_t ticks_to_wait);

    /*
      Reads the NVM memory from the STUSB4500
    */
    esp_err_t STUSB_read(STUSB4500_s *device);

    /*
      Write NVM settings to the STUSB4500. Optional: Passing a 255 value to the function will write
      	the default NVM values to the STUSB4500.
    */
    esp_err_t STUSB_write(STUSB4500_s *device, uint8_t defaultVals);

    /*
      Returns the voltage stored for the three power data objects (PDO).
      	Parameter: pdo_numb - the PDO number to be read (1 to 3).	
          1 - PDO1 (fixed at 5V)	
          2 - PDO2 (0-20V, 20mV resolution)	
          3 - PDO3 (0-20V, 20mV resolution)
    */
    float STUSB_get_voltage(STUSB4500_s *device,uint8_t pdo_numb);

    /*
      Returns the current stored for the three power data objects (PDO).
      	Parameter: pdo_numb - the PDO number to be read (1 to 3).
    */
    float STUSB_get_current(STUSB4500_s *device,uint8_t pdo_numb);

    /*
      Retruns the over voltage lock out variable (5-20%)
      	Parameter: pdo_numb - the PDO number to be read (1 to 3).
    */
    uint8_t STUSB_get_upper_voltage_limit(STUSB4500_s *device,uint8_t pdo_numb);

    /*
      Retruns the under voltage lock out variable (5-20%)
      	Note: PDO1 has a fixed threshold of 3.3V.
      	Parameter: pdo_numb - the PDO number to be read (1 to 3).
    */
    uint8_t STUSB_get_lower_voltage_limit(STUSB4500_s *device,uint8_t pdo_numb);

    /*
      Returns the global current value common to all PDO numbers.
    */
    float STUSB_get_flex_current(STUSB4500_s *device);

    /*
      Returns the number of sink PDOs
      	0,1 - PDO1
        2   - PDO2
        3   - PDO3
    */
    uint8_t STUSB_get_pdo_numbers(STUSB4500_s *device);

    /*
      Returns the SNK_UNCONS_POWER parameter value.
      	0 - No external source of power
        1 - An external power source is available and is sufficient to adequately power the system while charging external devices.
    */
    uint8_t STUSB_get_external_power(STUSB4500_s *device);

    /*
      Returns the USB_COMM_CAPABLE parameter value.	
      0 - Sink does not support data communication
      1 - Sink does support data communication
    */
    uint8_t STUSB_get_usb_comm_capable(STUSB4500_s *device);

    /*
      Returns the POWER_OK_CFG parameter value.	
      0 - Configuration 1
      1 - Not applicable
      2 - Configuration 2 (default)
      3 - Configuration 3
      
      Configuration 1:
      - VBUS_EN_SNK: Hi-Z - No source attached
                        0 - Source attached
      - POWER_OK2:   Hi-Z - No functionality
      - POWER_OK3:   Hi-Z - No functionality
      
      Configuration 2 (defualt):
      - VBUS_EN_SNK: Hi-Z - No source attached
                        0 - Source attached
      - POWER_OK2:   Hi-Z - No PD explicit contract
                        0 - PD explicit contract with PDO2
      - POWER_OK3:   Hi-Z - No PD explicit contract
                        0 - PD explicit contract with PDO3
    
      Configuration 3:
      - VBUS_EN_SNK: Hi-Z - No source attached
                        0 - source attached
      - POWER_OK2:   Hi-Z - No source attached or source supplies default
                            USB Type-C current at 5V when source attached.
                            					  0 - Source supplies 3.0A USB Type-C current at 5V
                                        when source is attached.
      - POWER_OK3:   Hi-Z - No source attached or source supplies default
                            USB Type-C current at 5V when source attached.
                                        0 - Source supplies 1.5A USB Type-C current at 5V
                                        when source is attached.
    */
    uint8_t STUSB_get_config_ok_gpio(STUSB4500_s *device);

    /*
      Returns the GPIO pin configuration.	
        0 - SW_CTRL_GPIO	
        1 - ERROR_RECOVERY	
        2 - DEBUG	
        3 - SINK_POWER		
        
        SW_CTRL_GPIO:
        - Software controlled GPIO. The output state is defined by the value
          of I2C register bit-0 at address 0x2D.	  	  
          
          Hi-Z - When bit-0 value is 0 (at start-up)	     
             0 - When bit-0 value is 1

      	ERROR_RECOVERY:	
        - Hardware fault detection (i.e. overtemperature is detected, overvoltage is	 
          detected on the CC pins, or after a hard reset the power delivery communication	  
          with the source is broken).	  	  
          
          Hi-Z - No hardware fault detected	     
             0 - Hardware fault detected		
        
        DEBUG:	
        - Debug accessory detection	  	  
          
          Hi-Z - No debug accessory detected	     
             0 - debug accessory detected		
        
        SINK_POWER:	
        - Indicates USB Type-C current capability advertised by the source.	  	  
          
          Hi-Z - Source supplies default or 1.5A USB Type-C current at 5V
             0 - Source supplies 3.0A USB Type-C current at 5V
    */
    uint8_t STUSB_get_gpio_ctrl(STUSB4500_s *device);

    /*
      Returns the POWER_ONLY_ABOVE_5V parameter configuration.	

        0 - VBUS_EN_SNK pin enabled when source is attached whatever VBUS_EN_SNK
            voltage (5V or any PDO voltage)

        1 - VBUS_EN_SNK pin enabled only when source attached and VBUS voltage
            negotiated to PDO2 or PDO3 voltage
    */
    uint8_t STUSB_get_power_above_5v_only(STUSB4500_s *device);

    /*
      Return the REQ_SRC_CURRENT parameter configuration. In case of match, selects
      	which operation current from the sink or the source is to be requested in the	
        RDO message.	
        0 - Request I(SNK_PDO) as operating current in RDO message	
        1 - Request I(SRC_PDO) as operating current in RDO message
    */
    uint8_t STUSB_get_req_src_current(STUSB4500_s *device);

    /*
      Sets the voltage to be requested for each of the three power data objects (PDO).	
        Parameter: 
          pdo_numb - the PDO number to be read (1 to 3).	           
          voltage  - the voltage to write to the PDO number.
      
        Note: PDO1 - Fixed at 5V	      
              PDO2 - 5-20V, 20mV resolution		  
              PDO3 - 5-20V, 20mV resolution
    */  
    esp_err_t STUSB_set_voltage(STUSB4500_s *device, uint8_t pdo_numb, float voltage);

    /*
      Sets the current value to be requested for each of the three power data objects (PDO).
      	Parameter: 
          pdo_numb - the PDO number to be read (1 to 3).	           
          current  - the current to write to the PDO number.

        Note: Valid current values are:	0.00*, 0.50, 0.75, 1.00, 1.25, 1.50, 1.75, 2.00, 
                                        2.25 , 2.50, 2.75, 3.00, 3.50, 4.00, 4.50, 5.00		
              *A value of 0 will use the FLEX_I value instead
    */
    esp_err_t STUSB_set_current(STUSB4500_s *device, uint8_t pdo_numb, float current);

    /*
      Sets the over voltage lock out parameter for each of the three power data objects (PDO).
      	Parameter: 
          pdo_numb - the PDO number to be read (1 to 3).
          value    - the coefficent to shift up nominal VBUS high voltage limit
                     to the PDO number.	
        
        Note: Valid high voltage limits are 5-20% in 1% increments
    */
    esp_err_t STUSB_set_upper_voltage_limit(STUSB4500_s *device, uint8_t pdo_numb, uint8_t value);

    /*
      Sets the under voltage lock out parameter for each of the three power data objects (PDO).
      	Parameter: 
          pdo_numb - the PDO number to be read (1 to 3).
          value    - the coefficent to shift down nominal VBUS lower voltage limit
                     to the PDO number.	
                     
        Note: Valid high voltage limits are 5-20% in 1% increments. 	      
              PDO1 has a fixed lower limit to 3.3V.
    */
    esp_err_t STUSB_set_lower_voltage_limit(STUSB4500_s *device, uint8_t pdo_numb, uint8_t value);

    /*
      Set the flexible current value common to all PDOs.	
        Parameter: 
          value - the current value to set to the FLEX_I parameter.	                   
                  (0-5A, 10mA resolution)
    */
    esp_err_t STUSB_set_flex_current(STUSB4500_s *device, float value);

    /*
      Sets the number of sink PDOs.	
        Paramter: 
          value - Number of sink PDOs
          	0 - 1 PDO (5V only)	
            1 - 1 PDO (5V only)	
            2 - 2 PDOs (PDO2 has the highest priority, followed by PDO1)
            3 - 3 PDOs (PDO3 has the highest priority, followed by PDO2, and then PDO1).
    */
    esp_err_t STUSB_set_pdo_number(STUSB4500_s *device, uint8_t value);

    /*
      Sets the SNK_UNCONS_POWER parameter value.	
        Parameter: 
          value - Value to set to SNK_UNCONS_POWER	
                  0 - No external source of power	
                  1 - An external power source is available and is sufficient to 	    
                      adequately power the system while charging external devices.
    */
    esp_err_t STUSB_set_external_power(STUSB4500_s *device, uint8_t value);

    /*
      Sets the USB_COMM_CAPABLE parameter value.	
        Parameter: 
          value - Value to set to USB_COMM_CAPABLE	
                  0 - Sink does not support data communication
                  1 - Sink does support data communication
    */
    esp_err_t STUSB_set_usb_comm_capable(STUSB4500_s *device, uint8_t value);

    /*
      Sets the POWER_OK_CFG parameter value.	
        Parameter: 
          value - Value to set to POWER_OK_CFG	
                  0 - Configuration 1	
                  1 - Not applicable	
                  2 - Configuration 2 (default)	
                  3 - Configuration 3		
          
          Configuration 1:	
            - VBUS_EN_SNK: Hi-Z - No source attached	                  
                              0 - Source attached	
            - POWER_OK2:   Hi-Z - No functionality	
            - POWER_OK3:   Hi-Z - No functionality		
          
          Configuration 2 (defualt):	
            - VBUS_EN_SNK: Hi-Z - No source attached	                  
                              0 - Source attached
            - POWER_OK2:   Hi-Z - No PD explicit contract	                  
                              0 - PD explicit contract with PDO2	
            - POWER_OK3:   Hi-Z - No PD explicit contract	                  
                              0 - PD explicit contract with PDO3
    
          Configuration 3:
            - VBUS_EN_SNK: Hi-Z - No source attached
                              0 - source attached
            - POWER_OK2:   Hi-Z - No source attached or source supplies default
                                  USB Type-C current at 5V when source attached.					  
                              0 - Source supplies 3.0A USB Type-C current at 5V					     
                                   when source is attached.	
            - POWER_OK3:   Hi-Z - No source attached or source supplies default	                      
                                  USB Type-C current at 5V when source attached.				      
                              0 - Source supplies 1.5A USB Type-C current at 5V					      
                                  when source is attached.
    */
    esp_err_t STUSB_set_config_ok_gpio(STUSB4500_s *device, uint8_t value);

    /*
      Sets the GPIO pin configuration.	
        Paramter: 
          value - Value to set to GPIO_CFG	
            0 - SW_CTRL_GPIO	
            1 - ERROR_RECOVERY	
            2 - DEBUG	
            3 - SINK_POWER		
          
          SW_CTRL_GPIO:	
            - Software controlled GPIO. The output state is defined by the value	  
                of I2C register bit-0 at address 0x2D.	  	  
              
              Hi-Z - When bit-0 value is 0 (at start-up)	     
                 0 - When bit-0 value is 1

      	ERROR_RECOVERY:	
          - Hardware fault detection (i.e. overtemperature is detected, overvoltage is	  
            detected on the CC pins, or after a hard reset the power delivery communication	  
            with the source is broken).	  	  
            
          Hi-Z - No hardware fault detected	     
             0 - Hardware fault detected		
        
        DEBUG:	
          - Debug accessory detection	  	  
            
          Hi-Z - No debug accessory detected	     
             0 - debug accessory detected		
             
        SINK_POWER:	
          - Indicates USB Type-C current capability advertised by the source.	  	  
          
          Hi-Z - Source supplies defualt or 1.5A USB Type-C current at 5V	     
             0 - Source supplies 3.0A USB Type-C current at 5V
    */
    esp_err_t STUSB_set_gpio_ctrl(STUSB4500_s *device, uint8_t value);

    /*
      Sets the POWER_ONLY_ABOVE_5V parameter configuration.	
        Parameter: 
          value - Value to select VBUS_EN_SNK pin configuration	
          0 - VBUS_EN_SNK pin enabled when source is attached whatever VBUS_EN_SNK	    
              voltage (5V or any PDO voltage)
          1 - VBUS_EN_SNK pin enabled only when source attached and VBUS voltage	    
              negotiated to PDO2 or PDO3 voltage
    */
    esp_err_t STUSB_set_power_above_5v_only(STUSB4500_s *device, uint8_t value);

    /*
      Sets the REQ_SRC_CURRENT parameter configuration. In case of match, selects	
        which operation current from the sink or the source is to be requested in the	
        RDO message.	
        
        Parameter: 
          value - Value to set to REQ_SRC_CURRENT	
            0 - Request I(SNK_PDO) as operating current in RDO message	
            1 - Request I(SRC_PDO) as operating current in RDO message
    */
    esp_err_t STUSB_set_req_src_current(STUSB4500_s *device,uint8_t value);
    
    /*
    	Performs a soft reset to force the STUSB4500 to re-negotiate with the source.
    */
    esp_err_t STUSB_soft_reset(STUSB4500_s *device);

    /*
    	reads USB-C connection status

      #NOT IMPLEMENTED
    */
    uint16_t STUSB_read_usb_c_status(STUSB4500_s *device);

    /*
    	reads USB PD status

      #NOT IMPLEMENTED
    */
    uint32_t STUSB_read_usb_pd_status(STUSB4500_s *device);
    
    /*
      Performs normal i2c write operation
    */
    esp_err_t STUSB_write_usb_pd(STUSB4500_s *device, uint16_t Register, uint8_t *DataW, uint16_t Length);
    
    /*
      Performs normal i2c read operation
    */
    esp_err_t STUSB_read_usb_pd(STUSB4500_s *device, uint16_t Register, uint8_t *DataR, uint16_t Length);

    /*
      Prints all STUSB values to ESP_LOGI
    */
    esp_err_t STUSB_print_values(STUSB4500_s *device);
#endif // STUSB4500LQTR_H