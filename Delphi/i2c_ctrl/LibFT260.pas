unit LibFT260;

interface

uses
  Windows;

const
  FT260_OK = 0;
  FT260_INVALID_HANDLE = 1;
  FT260_DEVICE_NOT_FOUND = 2;
  FT260_DEVICE_NOT_OPENED = 3;
  FT260_DEVICE_OPEN_FAIL = 4;
  FT260_DEVICE_CLOSE_FAIL = 5;
  FT260_INCORRECT_INTERFACE = 6;
  FT260_INCORRECT_CHIP_MODE = 7;
  FT260_DEVICE_MANAGER_ERROR = 8;
  FT260_IO_ERROR = 9;
  FT260_INVALID_PARAMETER = 10;
  FT260_NULL_BUFFER_POINTER = 11;
  FT260_BUFFER_SIZE_ERROR = 12;
  FT260_UART_SET_FAIL = 13;
  FT260_RX_NO_DATA = 14;
  FT260_GPIO_WRONG_DIRECTION = 15;
  FT260_INVALID_DEVICE = 16;
  FT260_OTHER_ERROR = 17;

const
  FT260_I2C_NONE  = 0;
  FT260_I2C_START = $02;
  FT260_I2C_REPEATED_START = $03;
  FT260_I2C_STOP  = $04;
  FT260_I2C_START_AND_STOP = $06;

// FT260 General Functions
function FT260_CreateDeviceList(var lpdwNumDevs: LongInt): LongInt; stdcall;
function FT260_Open(iDevice: Integer; var pFt260Handle: THandle): LongInt; stdcall;
function FT260_OpenByVidPid(vid: Word; pid: Word; deviceIndex: LongInt; var pFt260Handle: THandle): LongInt; stdcall;
function FT260_Close(pFt260Handle: LongInt): LongInt; stdcall;

// FT260 I2C Functions
function FT260_I2CMaster_Init(pFt260Handle: THandle; kbps: LongWord): LongInt; stdcall;
function FT260_I2CMaster_Read(pFt260Handle: THandle; deviceAddress: Byte; flag: LongWord; lpBuffer: PChar; dwBytesToRead: LongWord; var lpdwBytesReturned: LongWord; wait_time: LongWord): LongInt; stdcall;
function FT260_I2CMaster_Write(pFt260Handle: THandle; deviceAddress: Byte; flag: LongWord; lpBuffer: PChar; dwBytesToWrite: LongWord; var lpdwBytesWritten: LongWord): LongInt; stdcall;
function FT260_I2CMaster_GetStatus(pFt260Handle: THandle; var status: Byte): LongInt; stdcall;
function FT260_I2CMaster_Reset(pFt260Handle: THandle): LongInt; stdcall;

implementation

// FT260 General Functions
function FT260_CreateDeviceList; external 'LibFT260.dll' name '_FT260_CreateDeviceList@4';
function FT260_Open; external 'LibFT260.dll' name '_FT260_Open@8';
function FT260_OpenByVidPid; external 'LibFT260.dll' name '_FT260_OpenByVidPid@16';
function FT260_Close; external 'LibFT260.dll' name '_FT260_Close@4';

// FT260 I2C Functions
function FT260_I2CMaster_Init; external 'LibFT260.dll' name '_FT260_I2CMaster_Init@8';
function FT260_I2CMaster_Read; external 'LibFT260.dll' name '_FT260_I2CMaster_Read@28';
function FT260_I2CMaster_Write; external 'LibFT260.dll' name '_FT260_I2CMaster_Write@24';
function FT260_I2CMaster_GetStatus; external 'LibFT260.dll' name '_FT260_I2CMaster_GetStatus@8';
function FT260_I2CMaster_Reset; external 'LibFT260.dll' name '_FT260_I2CMaster_Reset@4';

end.
