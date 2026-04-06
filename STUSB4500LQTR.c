#include "STUSB4500LQTR.h"

static const char *TAG = "STUSB4500-LOGGER";


uint32_t STUSB_read_pdo(STUSB4500_s *device, uint8_t pdo_numb);
esp_err_t STUSB_write_pdo(STUSB4500_s *device, uint8_t pdo_numb, uint32_t pdoData);
esp_err_t STUSB_cust_enter_write_mode(STUSB4500_s *device, unsigned char ErasedSector);
esp_err_t STUSB_cust_exit_test_mode(STUSB4500_s *device);
esp_err_t STUSB_cust_write_sector(STUSB4500_s *device, char SectorNum, unsigned char *SectorData);
esp_err_t STUSB_cust_read_sector(STUSB4500_s *device, char SectorNum, unsigned char *SectorData);


esp_err_t STUSB_begin(STUSB4500_s *device, uint8_t deviceAddress, int i2c_master_num, TickType_t ticks_to_wait)
{
  if (!device) {
    return ESP_ERR_INVALID_ARG; // Null check for the device pointer
  } 
  ESP_LOGI(TAG, "device not null");
  // Initialize all variables in the STUSB4500_s struct
  device->initialized = false;
  device->address = deviceAddress;
  device->i2c_master_num = i2c_master_num; // Default I2C master port number (adjust as needed)
  device->ticks_to_wait = ticks_to_wait; // Default wait time (1000 ms)
  memset(device->sector, 0, sizeof(device->sector)); // Initialize sector array to 0
  device->readSectors = 0;

  //ESP_LOGI(TAG, "device [%d][0x%X][%d][%f][%d][%d]",device->initialized,device->address,device->i2c_master_num
  //                                                 ,(float)device->ticks_to_wait, device->sector[0][0],device->readSectors);
  // Perform a dummy I2C transaction to check device availability
  uint8_t device_id[] = {0};
  esp_err_t err = STUSB_read_usb_pd(device,0x2F,device_id,sizeof(device_id));
  //ESP_LOGI(TAG, "err of STUSB_read_usb_pd(device,0x2F,device_id,sizeof(device_id[%d]) = %d",device_id[0],err);
  //i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  //i2c_master_start(cmd);
  //i2c_master_write_byte(cmd, (device->address << 1) | I2C_MASTER_WRITE, true);
  //i2c_master_stop(cmd);
  //esp_err_t err = i2c_master_cmd_begin(device->i2c_master_num, cmd, device->ticks_to_wait);
  //i2c_cmd_link_delete(cmd);

  vTaskDelay(pdMS_TO_TICKS(1000));

  if (err == ESP_OK) {
      // Clear all interrupts by reading (I²C multi-read command for instance) all 10 registers from address 0x0D to 0x16
      uint8_t interrupts[10];
      STUSB_read_usb_pd(device,PORT_STATUS_0,interrupts,sizeof(interrupts));

      uint8_t interrupt_mask_register = 0x00;
      STUSB_write_usb_pd(device,ALERT_STATUS_1_MASK,&interrupt_mask_register,1);

      if (device->readSectors == 0) {
        err = STUSB_read(device);
        if (err == ESP_OK) {
            ESP_LOGI(TAG,"Found device and updated local data");
            device->readSectors = 1;
        }else{
          ESP_LOGE(TAG,"Failed to read from device");
        }
      }
      device->initialized = true; // Mark the device as initialized
      return ESP_OK; // Device is online
  } else {
      ESP_LOGE(TAG,"Failed to find device at (0x%X)",device->address);
      return ESP_FAIL; // Device not found
  }
}

esp_err_t STUSB_read(STUSB4500_s *device)
{
  uint8_t Buffer[1];
  device->readSectors = 1;
  //Read Current Parameters
  //-Enter Read Mode
  //-Read Sector[x][-]
  //---------------------------------
  //Enter Read Mode
  Buffer[0] = FTP_CUST_PASSWORD;  /* Set Password 0x95->0x47*/
  STUSB_write_usb_pd(device,FTP_CUST_PASSWORD_REG,Buffer,1);

  Buffer[0]= 0; /* NVM internal controller reset 0x96->0x00*/
  STUSB_write_usb_pd(device,FTP_CTRL_0,Buffer,1);

  vTaskDelay(pdMS_TO_TICKS(2));//wait for cmd execution
  
  Buffer[0]= FTP_CUST_PWR | FTP_CUST_RST_N; /* Set PWR and RST_N bits 0x96->0xC0*/
  STUSB_write_usb_pd(device,FTP_CTRL_0,Buffer,1);

  //--- End of CUST_EnterReadMode

  for(uint8_t i=0;i<5;i++)
  {
    Buffer[0] = FTP_CUST_PWR | FTP_CUST_RST_N; /* Set PWR and RST_N bits 0x96->0xC0*/
    STUSB_write_usb_pd(device,FTP_CTRL_0,Buffer,1);

    Buffer[0] = (READ & FTP_CUST_OPCODE);  /* Set Read Sectors Opcode 0x97->0x00*/
    STUSB_write_usb_pd(device,FTP_CTRL_1,Buffer,1);

    Buffer[0] = (i & FTP_CUST_SECT) | FTP_CUST_PWR | FTP_CUST_RST_N | FTP_CUST_REQ;
    STUSB_write_usb_pd(device,FTP_CTRL_0,Buffer,1);  /* Load Read Sectors Opcode */

    vTaskDelay(pdMS_TO_TICKS(2));//wait for cmd execution

    do 
    {
      STUSB_read_usb_pd(device,FTP_CTRL_0,Buffer,1); /* Wait for execution */
    }
    while(Buffer[0] & FTP_CUST_REQ); //The FTP_CUST_REQ is cleared by NVM controller when the operation is finished.

    esp_err_t err = STUSB_read_usb_pd(device,RW_BUFFER,&device->sector[i][0],8);
    //ESP_LOGI(TAG,"Error: %s",esp_err_to_name(err));
  }
  
  STUSB_cust_exit_test_mode(device);

  //for (int i = 0; i < 5; i++)
  //{
  //  for (int y = 0; y < 8; y++)
  //  {
  //    ESP_LOGI(TAG,"Bank[%d][%d]=0x%X",i,y,(unsigned int)device->sector[i][y]);
  //  }
  //  
  //}
  

  // NVM settings get loaded into the volatile registers after a hard reset or power cycle.
  // Below we will copy over some of the saved NVM settings to the I2C registers
  uint8_t currentValue;

  //PDO Number
  STUSB_set_pdo_number(device,(device->sector[3][2] & 0x06)>>1);


  //PDO1 - fixed at 5V and is unable to change
  STUSB_set_voltage(device,1,5.0);

  currentValue = (device->sector[3][2]&0xF0) >> 4;
  if(currentValue == 0)      STUSB_set_current(device,1,0);
  else if(currentValue < 11) STUSB_set_current(device,1,currentValue * 0.25 + 0.25);
  else                       STUSB_set_current(device,1,currentValue * 0.50 - 2.50);

  //PDO2
  STUSB_set_voltage(device,2,((device->sector[4][1]<<2) + (device->sector[4][0]>>6))/20.0);

  currentValue = (device->sector[3][4]&0x0F);
  if(currentValue == 0)      STUSB_set_current(device,2,0);
  else if(currentValue < 11) STUSB_set_current(device,2,currentValue * 0.25 + 0.25);
  else                       STUSB_set_current(device,2,currentValue * 0.50 - 2.50);

  //PDO3
  STUSB_set_voltage(device,3,(((device->sector[4][3]&0x03)<<8) + device->sector[4][2])/20.0);

  currentValue = (device->sector[3][5]&0xF0) >> 4;
  if(currentValue == 0)      STUSB_set_current(device,3,0);
  else if(currentValue < 11) STUSB_set_current(device,3,currentValue * 0.25 + 0.25);
  else                       STUSB_set_current(device,3,currentValue * 0.50 - 2.50);

  return ESP_OK;
}

esp_err_t STUSB_print_values(STUSB4500_s *device){
  ESP_LOGI(TAG,"STUSB VALUE OUTPUT");
    ESP_LOGI(TAG,"PDO Number: %d",STUSB_get_pdo_numbers(device));
    for (int i = 0; i < 3; i++)
    {
        ESP_LOGI(TAG,"_______ reading pdo %d ________",i+1);
        ESP_LOGI(TAG,"Voltage (V): %f",STUSB_get_voltage(device,i+1));
        ESP_LOGI(TAG,"Current (A): %f",STUSB_get_current(device,i+1));
        ESP_LOGI(TAG,"Upper Voltage Tolerance (A): %d",STUSB_get_upper_voltage_limit(device,i+1));
        ESP_LOGI(TAG,"Lower Voltage Tolerance (A): %d",STUSB_get_lower_voltage_limit(device,i+1));
    }
    /* Read the flex current value (FLEX_I) */
    ESP_LOGI(TAG,"Flex Current: %f",STUSB_get_flex_current(device));
    /* Read the External Power capable bit */
    ESP_LOGI(TAG,"External Power: %d",STUSB_get_external_power(device));
    /* Read the USB Communication capable bit */
    ESP_LOGI(TAG,"USB Communication Capable: %d",STUSB_get_usb_comm_capable(device));
    /* Read the POWER_OK pins configuration */
    ESP_LOGI(TAG,"Configuration OK GPIO: %d",STUSB_get_config_ok_gpio(device));
    /* Read the GPIO pin configuration */
    ESP_LOGI(TAG,"GPIO Control: %d",STUSB_get_gpio_ctrl(device));
    /* Read the bit that enables VBUS_EN_SNK pin only when power is greater than 5V */
    ESP_LOGI(TAG,"Enable Power Only Above 5V: %d",STUSB_get_power_above_5v_only(device));
        /* Read bit that controls if the Source or Sink device's operating current is used in the RDO message */
    ESP_LOGI(TAG,"Request Source Current: %d",STUSB_get_req_src_current(device));
    ESP_LOGI(TAG,"END OF STUSB VALUE OUTPUT");
    return ESP_OK;
}

esp_err_t STUSB_write(STUSB4500_s *device, uint8_t defaultVals)
{
  if(defaultVals != DEFAULT)
  {
  	uint8_t nvmCurrent[] = { 0, 0, 0};
  	float voltage[] = { 0, 0, 0};

  	uint32_t digitalVoltage=0;

  	//Load current values into NVM
  	for(uint8_t i=0; i<3; i++)
  	{
  	  uint32_t pdoData = STUSB_read_pdo(device,i+1);
  	  float current = (pdoData&0x3FF)*0.01; //The current is the first 10-bits of the 32-bit PDO register (10mA resolution)

  	  if(current > 5.0) current = 5.0; //Constrain current value to 5A max

  	  /*Convert current from float to 4-bit value
       -current from 0.5-3.0A is set in 0.25A steps
       -current from 3.0-5.0A is set in 0.50A steps
      */
  	  if(current < 0.5)     nvmCurrent[i] = 0;
  	  else if(current <= 3) nvmCurrent[i] = (4*current)-1;
  	  else                  nvmCurrent[i] = (2*current)+5;


  	  digitalVoltage = (pdoData>>10)&0x3FF; //The voltage is bits 10:19 of the 32-bit PDO register
  	  voltage[i] = digitalVoltage/20.0; //Voltage has 50mV resolution

  	  // Make sure the minimum voltage is between 5-20V
  	  if(voltage[i] < 5.0)       voltage[i] = 5.0;
  	  else if(voltage[i] > 20.0) voltage[i] = 20.0;
  	}

  	// load current for PDO1 (sector 3, byte 2, bits 4:7)
    device->sector[3][2] &= 0x0F;             //clear bits 4:7
    device->sector[3][2] |= (nvmCurrent[0]<<4);    //load new amperage for PDO1

    // load current for PDO2 (sector 3, byte 4, bits 0:3)
    device->sector[3][4] &= 0xF0;             //clear bits 0:3
    device->sector[3][4] |= nvmCurrent[1];     //load new amperage for PDO2

    // load current for PDO3 (sector 3, byte 5, bits 4:7)
    device->sector[3][5] &= 0x0F;           //clear bits 4:7
    device->sector[3][5] |= (nvmCurrent[2]<<4);  //set amperage for PDO3

    // The voltage for PDO1 is 5V and cannot be changed

    // PDO2
	  // Load voltage (10-bit)
	  // -bit 9:2 - sector 4, byte 1, bits 0:7
	  // -bit 0:1 - sector 4, byte 0, bits 6:7	
	  digitalVoltage = voltage[1] * 20;          //convert votlage to 10-bit value
	  device->sector[4][0] &= 0x3F;                        //clear bits 6:7
	  device->sector[4][0] |= ((digitalVoltage&0x03)<<6);  //load voltage bits 0:1 into bits 6:7
	  device->sector[4][1] = (digitalVoltage>>2);          //load bits 2:9

    // PDO3
    // Load voltage (10-bit)
    // -bit 8:9 - sector 4, byte 3, bits 0:1
    // -bit 0:7 - sector 4, byte 2, bits 0:7
    digitalVoltage = voltage[2] * 20;   //convert voltage to 10-bit value
    device->sector[4][2] = 0xFF & digitalVoltage; //load bits 0:7
    device->sector[4][3] &= 0xFC;                 //clear bits 0:1
    device->sector[4][3] |= (digitalVoltage>>8);  //load bits 8:9

    
    // Load highest priority PDO number from memory
    uint8_t Buffer[1];
    STUSB_read_usb_pd(device,DPM_PDO_NUMB, Buffer,1);
  
    //load PDO number (sector 3, byte 2, bits 2:3) for NVM saving
    device->sector[3][2] &= 0xF9;
    device->sector[3][2] |= (Buffer[0]<<1);


	  STUSB_cust_enter_write_mode(device,SECTOR_0 | SECTOR_1  | SECTOR_2 | SECTOR_3  | SECTOR_4 );
    STUSB_cust_write_sector(device,0,&device->sector[0][0]);
    STUSB_cust_write_sector(device,1,&device->sector[1][0]);
    STUSB_cust_write_sector(device,2,&device->sector[2][0]);
    STUSB_cust_write_sector(device,3,&device->sector[3][0]);
    STUSB_cust_write_sector(device,4,&device->sector[4][0]);
    STUSB_cust_exit_test_mode(device);
  }
  else
  {
	  uint8_t default_sector[5][8] = 
    {
      {0x00,0x00,0xB0,0xAA,0x00,0x45,0x00,0x00},
      {0x10,0x40,0x9C,0x1C,0xFF,0x01,0x3C,0xDF},
      {0x02,0x40,0x0F,0x00,0x32,0x00,0xFC,0xF1},
      {0x00,0x19,0x56,0xAF,0xF5,0x35,0x5F,0x00},
      {0x00,0x4B,0x90,0x21,0x43,0x00,0x40,0xFB}
    };
  
	  STUSB_cust_enter_write_mode(device, SECTOR_0 | SECTOR_1  | SECTOR_2 | SECTOR_3  | SECTOR_4 );
    STUSB_cust_write_sector(device,0,&default_sector[0][0]);
    STUSB_cust_write_sector(device,1,&default_sector[1][0]);
    STUSB_cust_write_sector(device,2,&default_sector[2][0]);
    STUSB_cust_write_sector(device,3,&default_sector[3][0]);
    STUSB_cust_write_sector(device,4,&default_sector[4][0]);
    STUSB_cust_exit_test_mode(device);
  }
  return ESP_OK;
}

float STUSB_get_voltage(STUSB4500_s *device,uint8_t pdo_numb)
{
    float voltage=0;
    uint32_t pdoData = STUSB_read_pdo(device, pdo_numb);

    pdoData = (pdoData>>10)&0x3FF;
    voltage = pdoData / 20.0;

    return voltage;
}

float STUSB_get_current(STUSB4500_s *device,uint8_t pdo_numb)
{
    uint32_t pdoData = STUSB_read_pdo(device, pdo_numb);

    pdoData &= 0x3FF;

    return pdoData * 0.01;
}

uint8_t STUSB_get_lower_voltage_limit(STUSB4500_s *device,uint8_t pdo_numb)
{  
    if(pdo_numb == 1) //PDO1
    {
    	return 0;
    }
    else if(pdo_numb == 2) //PDO2
    {
    	return (device->sector[3][4]>>4) + 5;
    }
    else //PDO3
    {
    	return (device->sector[3][6] & 0x0F) + 5;
    }
}

uint8_t STUSB_get_upper_voltage_limit(STUSB4500_s *device,uint8_t pdo_numb){
    if(pdo_numb == 1) //PDO1
    {
    	return (device->sector[3][3]>>4) + 5;
    }
    else if(pdo_numb == 2) //PDO2
    {
    	return (device->sector[3][5] & 0x0F) + 5;
    }
    else //PDO3
    {
    	return (device->sector[3][6]>>4) + 5;
    }
}

float STUSB_get_flex_current(STUSB4500_s *device)
{
  uint16_t digitalValue = ((device->sector[4][4]&0x0F)<<6) + ((device->sector[4][3]&0xFC)>>2);
  return (float)digitalValue / 100.0;
}

uint8_t STUSB_get_pdo_numbers(STUSB4500_s *device)
{
  uint8_t Buffer[1];
  STUSB_read_usb_pd(device,DPM_PDO_NUMB, Buffer, 1);//TODO MABY

  return Buffer[0]&0x07;
}

uint8_t STUSB_get_external_power(STUSB4500_s *device)
{
  return (device->sector[3][2]&0x08)>>3;
}

uint8_t STUSB_get_usb_comm_capable(STUSB4500_s *device)
{
  return (device->sector[3][2]&0x01);
}

uint8_t STUSB_get_config_ok_gpio(STUSB4500_s *device)
{
  return (device->sector[4][4]&0x60)>>5;
}

uint8_t STUSB_get_gpio_ctrl(STUSB4500_s *device)
{
  return (device->sector[1][0]&0x30)>>4;
}

uint8_t STUSB_get_power_above_5v_only(STUSB4500_s *device)
{
  return (device->sector[4][6]&0x08)>>3;
}

uint8_t STUSB_get_req_src_current(STUSB4500_s *device)
{
  return (device->sector[4][6]&0x10)>>4;
}

esp_err_t STUSB_set_voltage(STUSB4500_s *device, uint8_t pdo_numb, float voltage){
    
    if(pdo_numb < 1) pdo_numb = 1;
    else if(pdo_numb > 3) pdo_numb = 3;

    //Constrain voltage variable to 5-20V
    if(voltage < 5) voltage = 5;
    else if(voltage > 20) voltage = 20;

    // Load voltage to volatile PDO memory (PDO1 needs to remain at 5V)
    if(pdo_numb == 1) voltage = 5;
    
    voltage *= 20; 

    //Replace voltage from bits 10:19 with new voltage
    uint32_t pdoData = STUSB_read_pdo(device,pdo_numb);

    pdoData &= ~(0xFFC00);
    pdoData |= ((uint32_t)voltage<<10);

    STUSB_write_pdo(device,pdo_numb, pdoData);
    return ESP_OK;//TODO CHECK FOR ERRORS
}

esp_err_t STUSB_set_current(STUSB4500_s *device, uint8_t pdo_numb, float current){
    // Load current to volatile PDO memory
    current /= 0.01;

    uint32_t intCurrent = current;
    intCurrent &= 0x3FF;

    uint32_t pdoData = STUSB_read_pdo(device,pdo_numb);
    pdoData &= ~(0x3FF);//zero bottom 10 bits
    pdoData |= intCurrent;//set bottom 10 bits to selected current

    STUSB_write_pdo(device, pdo_numb, pdoData);

    return ESP_OK;//TODO CHECK FOR ERRORS
}

esp_err_t STUSB_set_lower_voltage_limit(STUSB4500_s *device, uint8_t pdo_numb, uint8_t value){
    //Constrain value to 5-20%
    if(value < 5) value = 5;
    else if(value > 20) value = 20;
    
    //UVLO1 fixed

    if(pdo_numb == 2) //UVLO2
    {
      //load UVLO (sector 3, byte 4, bits 4:7)
      device->sector[3][4] &= 0x0F;             //clear bits 4:7
      device->sector[3][4] |= (value-5)<<4;  //load new UVLO value
    }
    else if(pdo_numb == 3) //UVLO3
    {
      //load UVLO (sector 3, byte 6, bits 0:3)
      device->sector[3][6] &= 0xF0;
      device->sector[3][6] |= (value-5);
    }
    
    return ESP_OK;
}

esp_err_t STUSB_set_upper_voltage_limit(STUSB4500_s *device, uint8_t pdo_numb, uint8_t value){
    //Constrain value to 5-20%
    if(value < 5) value = 5;
    else if(value > 20) value = 20;

    if(pdo_numb == 1) //OVLO1
    {
      //load OVLO (sector 3, byte 3, bits 4:7)
      device->sector[3][3] &= 0x0F;             //clear bits 4:7
      device->sector[3][3] |= (value-5)<<4;  //load new OVLO value
    }
    else if(pdo_numb == 2) //OVLO2
    {
      //load OVLO (sector 3, byte 5, bits 0:3)
      device->sector[3][5] &= 0xF0;             //clear bits 0:3
      device->sector[3][5] |= (value-5);     //load new OVLO value
    }
    else if(pdo_numb == 3) //OVLO3
    {
      //load OVLO (sector 3, byte 6, bits 4:7)
      device->sector[3][6] &= 0x0F;
      device->sector[3][6] |= ((value-5)<<4);
    }
    return ESP_OK;
}

esp_err_t STUSB_set_flex_current(STUSB4500_s *device, float value){
    
    //Constrain value to 0-5A
    if(value > 5) value = 5;
    else if(value < 0) value = 0;
    
    uint16_t flex_val = value*100;

    device->sector[4][3] &= 0x03;                 //clear bits 2:6
    device->sector[4][3] |= ((flex_val&0x3F)<<2); //set bits 2:6
    
    device->sector[4][4] &= 0xF0;                 //clear bits 0:3
    device->sector[4][4] |= ((flex_val&0x3C0)>>6);//set bits 0:3
    return ESP_OK;
}

esp_err_t STUSB_set_pdo_number(STUSB4500_s *device, uint8_t value){
    uint8_t Buffer[2];
    if(value > 3) value = 3;

    //load PDO number to volatile memory
    Buffer[0] = value;
    STUSB_write_usb_pd(device, DPM_PDO_NUMB, Buffer,1);
    return ESP_OK;
}

esp_err_t STUSB_set_external_power(STUSB4500_s *device, uint8_t value){
    if(value != 0) value = 1;
  
    //load SNK_UNCONS_POWER (sector 3, byte 2, bit 3)
    device->sector[3][2] &= 0xF7; //clear bit 3
    device->sector[3][2] |= (value)<<3;
    
    return ESP_OK;
}

esp_err_t STUSB_set_usb_comm_capable(STUSB4500_s *device, uint8_t value)
{
    if(value != 0) value = 1;
    
    //load USB_COMM_CAPABLE (sector 3, byte 2, bit 0)
    device->sector[3][2] &= 0xFE; //clear bit 0
    device->sector[3][2] |= (value);
    return ESP_OK;
}

esp_err_t STUSB_set_config_ok_gpio(STUSB4500_s *device, uint8_t value){
    if(value < 2) value = 0;
    else if(value > 3) value = 3;
    
    //load POWER_OK_CFG (sector 4, byte 4, bits 5:6)
    device->sector[4][4] &= 0x9F; //clear bit 3
    device->sector[4][4] |= value<<5;
    return ESP_OK;
}

esp_err_t STUSB_set_gpio_ctrl(STUSB4500_s *device, uint8_t value) {
    if(value > 3) value = 3;
  
    //load GPIO_CFG (sector 1, byte 0, bits 4:5)
    device->sector[1][0] &= 0xCF; //clear bits 4:5
    device->sector[1][0] |= value<<4;
    return ESP_OK;
}

esp_err_t STUSB_set_power_above_5v_only(STUSB4500_s *device, uint8_t value){
    if(value != 0) value = 1;
  
    //load POWER_ONLY_ABOVE_5V (sector 4, byte 6, bit 3)
    device->sector[4][6] &= 0xF7; //clear bit 3
    device->sector[4][6] |= (value<<3); //set bit 3
    return ESP_OK;
}

esp_err_t STUSB_set_req_src_current(STUSB4500_s *device,uint8_t value){
    if(value != 0) value = 1;
  
    //load REQ_SRC_CURRENT (sector 4, byte 6, bit 4)
    device->sector[4][6] &= 0xEF; //clear bit 4
    device->sector[4][6] |= (value<<4); //set bit 4

    return ESP_OK;
}

//TODOO
uint16_t STUSB_read_usb_c_status(STUSB4500_s *device){
    esp_err_t err = ESP_OK;

    return err;
}

//TODOO
uint32_t STUSB_read_usb_pd_status(STUSB4500_s *device){
    uint32_t RDO;
    uint8_t _rdo[4];
    esp_err_t err = ESP_OK;

    err = STUSB_read_usb_pd(device,RDO_REG_STATUS_0,_rdo,sizeof(_rdo));

    //Combine the 4 buffer bytes into one 32-bit integer
    for(uint8_t i=0; i<4; i++)
    {
      uint32_t tempData = _rdo[i];
      tempData = (tempData<<(i*8));
      RDO += tempData;
    }


    return err;
}

esp_err_t STUSB_soft_reset(STUSB4500_s *device){
    uint8_t Buffer[1];

    //Soft Reset
    Buffer[0] = 0x0D; //SOFT_RESET
    STUSB_write_usb_pd(device,TX_HEADER_LOW, Buffer,1);

    Buffer[0] = 0x26; //SEND_COMMAND
    STUSB_write_usb_pd(device,PD_COMMAND_CTRL, Buffer,1);

    return ESP_OK;//TODO CHECK FOR ERRORS
}

uint32_t STUSB_read_pdo(STUSB4500_s *device,uint8_t pdo_numb){
    uint32_t pdoData=0;
    uint8_t Buffer[4];

    if(pdo_numb == 0){
      pdo_numb++;
    }

    //PDO1:0x85, PDO2:0x89, PDO3:0x8D
    STUSB_read_usb_pd(device, 0x85 + ((pdo_numb-1)*4), Buffer, 4);

    //Combine the 4 buffer bytes into one 32-bit integer
    for(uint8_t i=0; i<4; i++)
    {
      uint32_t tempData = Buffer[i];
      tempData = (tempData<<(i*8));
      pdoData += tempData;
    }

    return pdoData;//TODO CHECK FOR ERRORS
}

esp_err_t STUSB_write_pdo(STUSB4500_s *device,uint8_t pdo_numb, uint32_t pdoData){
    uint8_t Buffer[4];

    if(pdo_numb == 0){
      pdo_numb++;
    }

    Buffer[0] = (pdoData)    & 0xFF;
    Buffer[1] = (pdoData>>8) & 0xFF;
    Buffer[2] = (pdoData>>16)& 0xFF;
    Buffer[3] = (pdoData>>24)& 0xFF;

    STUSB_write_usb_pd(device,0x85 + ((pdo_numb-1)*4), Buffer, 4);
    return ESP_OK;//TODO CHECK FOR ERRORS
}

esp_err_t STUSB_cust_enter_write_mode(STUSB4500_s *device,unsigned char ErasedSector){
    uint8_t Buffer[1];
  
    Buffer[0] = FTP_CUST_PASSWORD;   /* Set Password */
    if ( STUSB_write_usb_pd(device,FTP_CUST_PASSWORD_REG,Buffer,1) != ESP_OK) return ESP_FAIL;
    
    Buffer[0] = 0;   /* this register must be NULL for Partial Erase feature */
    if ( STUSB_write_usb_pd(device,RW_BUFFER,Buffer,1) != ESP_OK) return ESP_FAIL;
    
    
    //NVM Power-up Sequence
    //After STUSB start-up sequence, the NVM is powered off.

    Buffer[0] = 0;  /* NVM internal controller reset */
    if ( STUSB_write_usb_pd(device,FTP_CTRL_0,Buffer,1) != 0 ) return ESP_FAIL;
    
    vTaskDelay(pdMS_TO_TICKS(2));//wait 2ms for cmd execution

    Buffer[0] = FTP_CUST_PWR | FTP_CUST_RST_N; /* Set PWR and RST_N bits */
    if ( STUSB_write_usb_pd(device,FTP_CTRL_0,Buffer,1) != 0 ) return ESP_FAIL;

    
    Buffer[0] = ((ErasedSector << 3) & FTP_CUST_SER) | ( WRITE_SER & FTP_CUST_OPCODE);  /* Load 0xF1 to erase all sectors of FTP and Write SER Opcode */
    if ( STUSB_write_usb_pd(device,FTP_CTRL_1,Buffer,1) != ESP_OK) return ESP_FAIL; /* Set Write SER Opcode */
    
    Buffer[0] = FTP_CUST_PWR | FTP_CUST_RST_N | FTP_CUST_REQ; 
    if ( STUSB_write_usb_pd(device,FTP_CTRL_0,Buffer,1) != ESP_OK) return ESP_FAIL; /* Load Write SER Opcode */
    
    do 
    {
        vTaskDelay(pdMS_TO_TICKS(500));
        if ( STUSB_read_usb_pd(device,FTP_CTRL_0,Buffer,1) != ESP_OK) return ESP_FAIL; /* Wait for execution */
    }
    while(Buffer[0] & FTP_CUST_REQ); 
    
    Buffer[0] =  SOFT_PROG_SECTOR & FTP_CUST_OPCODE;  
    if ( STUSB_write_usb_pd(device,FTP_CTRL_1,Buffer,1) != ESP_OK) return ESP_FAIL;  /* Set Soft Prog Opcode */
    
    Buffer[0] = FTP_CUST_PWR | FTP_CUST_RST_N | FTP_CUST_REQ; 
    if ( STUSB_write_usb_pd(device,FTP_CTRL_0,Buffer,1) != ESP_OK) return ESP_FAIL; /* Load Soft Prog Opcode */

    do 
    {
      if ( STUSB_read_usb_pd(device,FTP_CTRL_0,Buffer,1) != ESP_OK) return ESP_FAIL; /* Wait for execution */
    }
    while(Buffer[0] & FTP_CUST_REQ);
    
    Buffer[0]= ERASE_SECTOR & FTP_CUST_OPCODE;  
    if ( STUSB_write_usb_pd(device,FTP_CTRL_1,Buffer,1) != ESP_OK) return ESP_FAIL; /* Set Erase Sectors Opcode */
    
    Buffer[0]=FTP_CUST_PWR | FTP_CUST_RST_N | FTP_CUST_REQ;  
    if ( STUSB_write_usb_pd(device,FTP_CTRL_0,Buffer,1) != ESP_OK) return ESP_FAIL; /* Load Erase Sectors Opcode */
    
    do 
    {
      if ( STUSB_read_usb_pd(device,FTP_CTRL_0,Buffer,1) != ESP_OK) return ESP_FAIL; /* Wait for execution */
    }
    while(Buffer[0] & FTP_CUST_REQ);  

    return ESP_OK;
}

esp_err_t STUSB_cust_exit_test_mode(STUSB4500_s *device){
    uint8_t Buffer[2];
  
    Buffer[0] = FTP_CUST_RST_N;
    Buffer[1] = 0x00;  /* clear registers */
    if ( STUSB_write_usb_pd(device,FTP_CTRL_0,Buffer,2) != ESP_OK) return ESP_FAIL;
    
    Buffer[0]= 0x00;
    if ( STUSB_write_usb_pd(device,FTP_CUST_PASSWORD_REG,Buffer,1) != ESP_OK) return ESP_FAIL;  /* Clear Password */
    
    return ESP_OK;
}

esp_err_t STUSB_cust_write_sector(STUSB4500_s *device,char SectorNum, unsigned char *SectorData){
    uint8_t Buffer[1];

    //Write the 64-bit data to be written in the sector
    if(STUSB_write_usb_pd(device,RW_BUFFER,SectorData,8) != ESP_OK) return ESP_FAIL;

    Buffer[0]= FTP_CUST_PWR | FTP_CUST_RST_N; /*Set PWR and RST_N bits*/
    if( STUSB_write_usb_pd(device,FTP_CTRL_0,Buffer,1) != ESP_OK) return ESP_FAIL;

    //NVM Program Load Register to write with the 64-bit data to be written in sector
    Buffer[0]= (WRITE_PL & FTP_CUST_OPCODE); /*Set Write to PL Opcode*/
    if ( STUSB_write_usb_pd(device,FTP_CTRL_1,Buffer,1) != ESP_OK) return ESP_FAIL;

    Buffer[0]=FTP_CUST_PWR |FTP_CUST_RST_N | FTP_CUST_REQ;  /* Load Write to PL Sectors Opcode */  
    if ( STUSB_write_usb_pd(device,FTP_CTRL_0,Buffer,1) != ESP_OK) return ESP_FAIL;

    do 
    {
        if ( STUSB_read_usb_pd(device,FTP_CTRL_0,Buffer,1) != ESP_OK) return ESP_FAIL; /* Wait for execution */
    }     
    while(Buffer[0] & FTP_CUST_REQ); //FTP_CUST_REQ clear by NVM controller

    //NVM "Word Program" operation to write the Program Load Register in the sector to be written
    Buffer[0]= (PROG_SECTOR & FTP_CUST_OPCODE);
    if ( STUSB_write_usb_pd(device,FTP_CTRL_1,Buffer,1) != ESP_OK) return ESP_FAIL;/*Set Prog Sectors Opcode*/

    Buffer[0]=(SectorNum & FTP_CUST_SECT) | FTP_CUST_PWR | FTP_CUST_RST_N | FTP_CUST_REQ;
    if ( STUSB_write_usb_pd(device,FTP_CTRL_0,Buffer,1) != ESP_OK) return ESP_FAIL; /* Load Prog Sectors Opcode */

    do 
    {
        if ( STUSB_read_usb_pd(device,FTP_CTRL_0,Buffer,1) != ESP_OK) return ESP_FAIL; /* Wait for execution */
    }
    while(Buffer[0] & FTP_CUST_REQ); //FTP_CUST_REQ clear by NVM controller
  
    return ESP_OK;
}

esp_err_t STUSB_cust_read_sector(STUSB4500_s *device, char SectorNum, unsigned char *SectorData){
  uint8_t Buffer[2];
    
  Buffer[0]= FTP_CUST_PWR | FTP_CUST_RST_N ;  /* Set PWR and RST_N bits */
  if ( STUSB_write_usb_pd(device,FTP_CTRL_0,Buffer,1) != ESP_OK ) return ESP_FAIL;
  
  Buffer[0]= (READ & FTP_CUST_OPCODE);
  if ( STUSB_write_usb_pd(device,FTP_CTRL_1,Buffer,1) != ESP_OK ) return ESP_FAIL;/* Set Read Sectors Opcode */
  
  Buffer[0]= (SectorNum & FTP_CUST_SECT) |FTP_CUST_PWR |FTP_CUST_RST_N | FTP_CUST_REQ;
  if ( STUSB_write_usb_pd(device,FTP_CTRL_0,Buffer,1) != ESP_OK ) return ESP_FAIL;  /* Load Read Sectors Opcode */
  
  do 
  {
      if ( STUSB_read_usb_pd(device,FTP_CTRL_0,Buffer,1) != ESP_OK ) return ESP_FAIL; /* Wait for execution */
  }
  while(Buffer[0] & FTP_CUST_REQ); //The FTP_CUST_REQ is cleared by NVM controller when the operation is finished.
  
  if ( STUSB_read_usb_pd(device,RW_BUFFER,&SectorData[0],8) != ESP_OK ) return ESP_FAIL; /* Sectors Data are available in RW-BUFFER @ 0x53 */
  
  Buffer[0] = 0 ; /* NVM internal controller reset */
  if ( STUSB_write_usb_pd(device,FTP_CTRL_0,Buffer,1) != ESP_OK ) return ESP_FAIL;
  
  return 0;
}

esp_err_t STUSB_write_usb_pd(STUSB4500_s *device,uint16_t Register, uint8_t *DataW, uint16_t Length){
    esp_err_t err = ESP_OK;
    uint8_t buffer[500] = { 0 };

    i2c_cmd_handle_t handle = i2c_cmd_link_create_static(buffer, sizeof(buffer));
    assert (handle != NULL);

    err = i2c_master_start(handle);
    if (err != ESP_OK) {
        goto end;
    }

    err = i2c_master_write_byte(handle, device->address << 1 | I2C_MASTER_WRITE, true);
    if (err != ESP_OK) {
        goto end;
    }

    err = i2c_master_write_byte(handle, Register, true);
    if (err != ESP_OK) {
        goto end;
    }

    err = i2c_master_write(handle, DataW, Length, true);
    if (err != ESP_OK) {
        goto end;
    }

    i2c_master_stop(handle);
    err = i2c_master_cmd_begin(device->i2c_master_num, handle, device->ticks_to_wait);

end:
    i2c_cmd_link_delete_static(handle);
    return err;
}

esp_err_t STUSB_read_usb_pd(STUSB4500_s *device,uint16_t Register, uint8_t *DataR, uint16_t Length){
    esp_err_t err = ESP_OK;
    uint8_t write_buffer[] = {Register};
    err = i2c_master_write_read_device(device->i2c_master_num,device->address,write_buffer,sizeof(write_buffer),DataR,Length,device->ticks_to_wait);
    
    return err;
}