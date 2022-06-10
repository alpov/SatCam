unit Unit1;

interface

uses
  Windows, Messages, SysUtils, Variants, Classes, Graphics, Controls, Forms,
  Dialogs, StdCtrls, ComCtrls, ExtCtrls, Buttons;

type
  TForm1 = class(TForm)
    Memo1: TMemo;
    Button1: TButton;
    Panel1: TPanel;
    Timer1: TTimer;
    GroupBox1: TGroupBox;
    GroupBox2: TGroupBox;
    GroupBox3: TGroupBox;
    GroupBox4: TGroupBox;
    RadioButton1: TRadioButton;
    RadioButton2: TRadioButton;
    RadioButton3: TRadioButton;
    RadioButton4: TRadioButton;
    Button4: TButton;
    Button5: TButton;
    Button6: TButton;
    Button7: TButton;
    Edit1: TEdit;
    Edit2: TEdit;
    Button8: TButton;
    RadioButton5: TRadioButton;
    RadioButton6: TRadioButton;
    RadioButton7: TRadioButton;
    RadioButton8: TRadioButton;
    Button9: TButton;
    RadioButton9: TRadioButton;
    Button10: TButton;
    RadioButton10: TRadioButton;
    RadioButton11: TRadioButton;
    RadioButton12: TRadioButton;
    RadioButton13: TRadioButton;
    RadioButton14: TRadioButton;
    RadioButton15: TRadioButton;
    RadioButton16: TRadioButton;
    RadioButton17: TRadioButton;
    RadioButton18: TRadioButton;
    RadioButton19: TRadioButton;
    RadioButton20: TRadioButton;
    Edit3: TEdit;
    GroupBox5: TGroupBox;
    Label1: TLabel;
    Label2: TLabel;
    Edit4: TEdit;
    Edit5: TEdit;
    Button11: TButton;
    Button12: TButton;
    Label3: TLabel;
    RadioButton21: TRadioButton;
    Label4: TLabel;
    RadioButton22: TRadioButton;
    RadioButton23: TRadioButton;
    RadioButton24: TRadioButton;
    RadioButton25: TRadioButton;
    RadioButton26: TRadioButton;
    RadioButton27: TRadioButton;
    SpeedButton1: TSpeedButton;
    Timer2: TTimer;
    procedure FormCreate(Sender: TObject);
    procedure FormDestroy(Sender: TObject);
    procedure Button1Click(Sender: TObject);
    procedure Timer1Timer(Sender: TObject);
    procedure CmdButtonClick(Sender: TObject);
    procedure Button11Click(Sender: TObject);
    procedure Button12Click(Sender: TObject);
    procedure Memo1Click(Sender: TObject);
    procedure Edit4Change(Sender: TObject);
    procedure SpeedButton1Click(Sender: TObject);
    procedure Timer2Timer(Sender: TObject);
  private
    hFT: THandle;
    procedure WriteCommand(Command: Byte; Param: Byte);
  end;

var
  Form1: TForm1;

const
  devAddr = $3c;
  numRegs = $30;
  regNames: array[0..numRegs-1] of string = (
    'command',
    ' save_page',
    'save_count',
    'save_delay',
    'save_light_lo',
    'save_light_hi',
    'spl_light_delay',
    'spl_volt_delay',
    'spl_temp_delay',
    'psk_append',
    ' value_light',
    'value_volt',
    'value_temp',
    '_dummy_1',
    '_dummy_1',
    '_dummy_1',
    ' cam_agc',
    'cam_aec',
    'cam_agc_ceiling',
    'cam_agc_manual',
    'cam_aec_manual',
    'cam_awb',
    'cam_rotate',
    ' psk_speed',
    'psk_freq',
    'cw_wpm',
    'cw_freq',
    '_dummy_2',
    '_dummy_2',
    '_dummy_2',
    '_dummy_2',
    '_dummy_2',
{
    ' callsign_0',
    'callsign_1',
    'callsign_2',
    'callsign_3',
    'callsign_4',
}
    '_dummy_2',
    '_dummy_2',
    '_dummy_2',
    '_dummy_2',
    '_dummy_2',
    ' sys_i2c_watchdog',
    'sys_autoreboot',
    ' cam_delay',
    'cam_qs',
    ' sstv_ampl',
    'psk_ampl',
    'cw_ampl',
    'debug_enable',
    'light_cal',
    '_dummy_3',
    '_dummy_3'
  );

implementation

{$R *.dfm}

uses LibFT260;

procedure TForm1.FormCreate(Sender: TObject);
var
  devNum: LongInt;
begin
  DecimalSeparator := '.';
  Randomize;

  FT260_CreateDeviceList(devNum);
  FT260_OpenByVidPid($0403, $6030, 0, hFT);
  FT260_I2CMaster_Init(hFT, 100);
	FT260_I2CMaster_Reset(hFT);

  Button1.Click;
  Edit4.OnChange(Self);
end;

procedure TForm1.FormDestroy(Sender: TObject);
begin
  FT260_Close(hFT);
end;

procedure TForm1.WriteCommand(Command: Byte; Param: Byte);
var
  b_written: LongWord;
  Cmd: array[0..2] of byte;
  StatusID: Byte;
begin
  Cmd[0] := 0;
  Cmd[1] := Command;
  Cmd[2] := Param;

  FT260_I2CMaster_Write(hFT, devAddr, FT260_I2C_START_AND_STOP, @Cmd, 3, b_written);
  Sleep(10);

  FT260_I2CMaster_GetStatus(hFT, StatusID);
  if (StatusID and $1F) <> 0 then begin
    Panel1.Color := clRed;
    Panel1.Caption := 'Error';
  end;
end;

procedure TForm1.Button1Click(Sender: TObject);
var
  b_written, b_read: LongWord;
  Cmd: Byte;
  Data: array[0..127] of Word;
  StatusID: Byte;
  I: Integer;
begin
  Cmd := $0;

  FT260_I2CMaster_Write(hFT, devAddr, FT260_I2C_START, @Cmd, 1, b_written);
  FT260_I2CMaster_Read(hFT, devAddr, FT260_I2C_REPEATED_START or FT260_I2C_STOP, @Data, numRegs*2, b_read, 5000);

  FT260_I2CMaster_GetStatus(hFT, StatusID);
  if (StatusID and $1F) <> 0 then Memo1.Lines.Add(Format('status error 0x%2x', [StatusID]));

  Memo1.Lines.Clear;
  for I := 0 to numRegs-1 do begin
    if regNames[I][1] = ' ' then Memo1.Lines.Add('-------------------------------------------');
    if Pos('dummy', regNames[I]) = 0 then Memo1.Lines.Add(Format('0x%.2x = %17s = 0x%.4x = %d', [(I+Cmd)*2, regNames[I], Data[I], Data[I]]));
  end;
  Memo1.Lines.Add('-------------------------------------------');
  Memo1.Lines.Add(Format('call = %17s = %s', ['callsign', PChar(@Data[$20])]));
end;

procedure TForm1.Timer1Timer(Sender: TObject);
var
  b_written, b_read: LongWord;
  Cmd: Byte;
  Data: Word;
  StatusID: Byte;
begin
  Cmd := $0;
  FT260_I2CMaster_Write(hFT, devAddr, FT260_I2C_START, @Cmd, 1, b_written);
  FT260_I2CMaster_Read(hFT, devAddr, FT260_I2C_REPEATED_START or FT260_I2C_STOP, @Data, 2, b_read, 5000);
  FT260_I2CMaster_GetStatus(hFT, StatusID);
  if (StatusID and $1F) <> 0 then begin
    Panel1.Color := clGray;
    Panel1.Caption := 'Error';
  end else if Data <> 0 then begin
    Panel1.Color := clRed;
    Panel1.Caption := 'Busy';
  end else begin
    Panel1.Color := clLime;
    Panel1.Caption := 'Ready';
  end;
end;

procedure TForm1.CmdButtonClick(Sender: TObject);
begin
  if (Sender = Button4) or (Sender = Button5) or (Sender = Button6) or (Sender = Button7) then begin
    if RadioButton1.Checked then WriteCommand((Sender as TButton).Tag, 0)
    else if RadioButton2.Checked then WriteCommand((Sender as TButton).Tag, StrToIntDef(Edit1.Text, 0) + 1)
    else if RadioButton3.Checked then WriteCommand((Sender as TButton).Tag, 17)
    else if RadioButton4.Checked then WriteCommand((Sender as TButton).Tag, StrToIntDef(Edit2.Text, 0) + 18);
  end else if Sender = Button8 then begin
    if RadioButton5.Checked then WriteCommand(5, 0)
    else if RadioButton6.Checked then WriteCommand(5, 1)
    else if RadioButton7.Checked then WriteCommand(5, 2)
    else if RadioButton21.Checked then WriteCommand(5, 3)
    else if RadioButton8.Checked then WriteCommand(5, 4)
    else if RadioButton22.Checked then WriteCommand(5, 5)
    else if RadioButton23.Checked then WriteCommand(5, 6)
    else if RadioButton24.Checked then WriteCommand(5, 7)
    else if RadioButton25.Checked then WriteCommand(5, 8)
    else if RadioButton26.Checked then WriteCommand(5, 9);
  end else if Sender = Button9 then begin
    WriteCommand(6, 0);
  end else if Sender = Button10 then begin
    if RadioButton10.Checked then WriteCommand(254, 0)
    else if RadioButton11.Checked then WriteCommand(254, 1)
    else if RadioButton12.Checked then WriteCommand(254, 2)
    else if RadioButton13.Checked then WriteCommand(254, 3)
    else if RadioButton14.Checked then WriteCommand(254, 4)
    else if RadioButton19.Checked then WriteCommand(254, 184)
    else if RadioButton27.Checked then WriteCommand(254, 185)
    else if RadioButton18.Checked then WriteCommand(254, 200)
    else if RadioButton17.Checked then WriteCommand(254, 201)
    else if RadioButton16.Checked then WriteCommand(254, 210)
    else if RadioButton15.Checked then WriteCommand(254, 220)
    else if RadioButton20.Checked then WriteCommand(254, StrToIntDef(Edit3.Text, 0) + 221);
  end
end;

procedure TForm1.Button11Click(Sender: TObject);
var
  b_written, b_read: LongWord;
  Cmd: Byte;
  Data: Word;
  StatusID: Byte;
begin
  Cmd := StrToIntDef(Edit4.Text, 0);

  FT260_I2CMaster_Write(hFT, devAddr, FT260_I2C_START, @Cmd, 1, b_written);
  FT260_I2CMaster_Read(hFT, devAddr, FT260_I2C_REPEATED_START or FT260_I2C_STOP, @Data, 2, b_read, 5000);

  FT260_I2CMaster_GetStatus(hFT, StatusID);
  if (StatusID and $1F) <> 0 then begin
    Panel1.Color := clRed;
    Panel1.Caption := 'Error';
  end;

  Edit5.Text := IntToStr(Data);
end;

procedure TForm1.Button12Click(Sender: TObject);
var
  b_written: LongWord;
  Cmd: array[0..2] of byte;
  StatusID: Byte;
begin
  Cmd[0] := StrToIntDef(Edit4.Text, 0);
  Cmd[1] := StrToIntDef(Edit5.Text, 0) shr 0;
  Cmd[2] := StrToIntDef(Edit5.Text, 0) shr 8;

  FT260_I2CMaster_Write(hFT, devAddr, FT260_I2C_START_AND_STOP, @Cmd, 3, b_written);
  Sleep(10);

  FT260_I2CMaster_GetStatus(hFT, StatusID);
  if (StatusID and $1F) <> 0 then begin
    Panel1.Color := clRed;
    Panel1.Caption := 'Error';
  end;
end;

procedure TForm1.Memo1Click(Sender: TObject);
var
  Line: Integer;
  b_written, b_read: LongWord;
  Cmd: Byte;
  Data: array[0..127] of Byte;
  S: string;
  StatusID: Byte;
  I: Integer;
begin
  Line := Memo1.Perform(EM_LINEFROMCHAR, Memo1.SelStart, 0);
  if Pos('call', Memo1.Lines[Line]) = 1 then begin
    Cmd := $40;
    FT260_I2CMaster_Write(hFT, devAddr, FT260_I2C_START, @Cmd, 1, b_written);
    FT260_I2CMaster_Read(hFT, devAddr, FT260_I2C_REPEATED_START or FT260_I2C_STOP, @Data, 10, b_read, 5000);
    FT260_I2CMaster_GetStatus(hFT, StatusID);
    if (StatusID and $1F) <> 0 then Exit;
    S := PChar(@Data[0]);
    if InputQuery('Callsign', 'Enter new callsign', S) then begin
      Data[0] := $40;
      for I := 1 to Length(S) do Data[I] := Ord(S[I]);
      Data[Length(S)+1] := 0;
      FT260_I2CMaster_Write(hFT, devAddr, FT260_I2C_START_AND_STOP, @Data, Length(S)+2, b_written);
      Sleep(10);
    end;
  end else if Memo1.Lines[Line][2] = 'x' then begin
    Edit4.Text := '$' + Memo1.Lines[Line][3] + Memo1.Lines[Line][4];
    Button11.Click;
  end;
end;

procedure TForm1.Edit4Change(Sender: TObject);
begin
  Label3.Caption := regNames[StrToIntDef(Edit4.Text, 0) div 2];
end;

procedure TForm1.SpeedButton1Click(Sender: TObject);
begin
  Timer2.Enabled := SpeedButton1.Down;
end;

procedure TForm1.Timer2Timer(Sender: TObject);
begin
  Button4.Click;
end;

end.
