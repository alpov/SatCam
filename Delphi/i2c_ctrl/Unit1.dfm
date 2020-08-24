object Form1: TForm1
  Left = 192
  Top = 107
  Width = 677
  Height = 684
  Caption = 'SatCam I2C control'
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  OnCreate = FormCreate
  OnDestroy = FormDestroy
  PixelsPerInch = 96
  TextHeight = 13
  object Memo1: TMemo
    Left = 5
    Top = 5
    Width = 341
    Height = 641
    Font.Charset = EASTEUROPE_CHARSET
    Font.Color = clWindowText
    Font.Height = -11
    Font.Name = 'Courier New'
    Font.Style = []
    Lines.Strings = (
      'Memo1')
    ParentFont = False
    ScrollBars = ssVertical
    TabOrder = 0
    OnClick = Memo1Click
  end
  object Button1: TButton
    Left = 360
    Top = 620
    Width = 146
    Height = 25
    Caption = 'Read memory'
    TabOrder = 1
    OnClick = Button1Click
  end
  object Panel1: TPanel
    Left = 545
    Top = 620
    Width = 110
    Height = 26
    Caption = 'Ready'
    Color = clLime
    TabOrder = 2
  end
  object GroupBox1: TGroupBox
    Left = 360
    Top = 10
    Width = 296
    Height = 111
    Caption = ' SSTV '
    TabOrder = 3
    object RadioButton1: TRadioButton
      Left = 100
      Top = 15
      Width = 113
      Height = 17
      Caption = 'live image'
      Checked = True
      TabOrder = 0
      TabStop = True
    end
    object RadioButton2: TRadioButton
      Left = 100
      Top = 35
      Width = 113
      Height = 17
      Caption = 'flash memory'
      TabOrder = 1
    end
    object RadioButton3: TRadioButton
      Left = 100
      Top = 55
      Width = 113
      Height = 17
      Caption = 'thumbnails'
      TabOrder = 2
    end
    object RadioButton4: TRadioButton
      Left = 100
      Top = 75
      Width = 113
      Height = 17
      Caption = 'hard-coded ROM'
      TabOrder = 3
    end
    object Button4: TButton
      Tag = 1
      Left = 10
      Top = 20
      Width = 75
      Height = 21
      Caption = 'Robot36'
      TabOrder = 4
      OnClick = CmdButtonClick
    end
    object Button5: TButton
      Tag = 2
      Left = 10
      Top = 40
      Width = 75
      Height = 21
      Caption = 'Robot72'
      TabOrder = 5
      OnClick = CmdButtonClick
    end
    object Button6: TButton
      Tag = 3
      Left = 10
      Top = 60
      Width = 75
      Height = 21
      Caption = 'MP73'
      TabOrder = 6
      OnClick = CmdButtonClick
    end
    object Button7: TButton
      Tag = 4
      Left = 10
      Top = 80
      Width = 75
      Height = 21
      Caption = 'MP115'
      TabOrder = 7
      OnClick = CmdButtonClick
    end
    object Edit1: TEdit
      Left = 210
      Top = 33
      Width = 46
      Height = 21
      TabOrder = 8
      Text = '0'
    end
    object Edit2: TEdit
      Left = 210
      Top = 73
      Width = 46
      Height = 21
      TabOrder = 9
      Text = '0'
    end
  end
  object GroupBox2: TGroupBox
    Left = 360
    Top = 125
    Width = 296
    Height = 106
    Caption = ' PSK '
    TabOrder = 4
    object Label4: TLabel
      Left = 200
      Top = 15
      Width = 75
      Height = 13
      Caption = 'samples all -- 32'
    end
    object Button8: TButton
      Left = 10
      Top = 20
      Width = 75
      Height = 21
      Caption = 'PSK'
      TabOrder = 0
      OnClick = CmdButtonClick
    end
    object RadioButton5: TRadioButton
      Left = 100
      Top = 15
      Width = 90
      Height = 17
      Caption = 'hello'
      Checked = True
      TabOrder = 1
      TabStop = True
    end
    object RadioButton6: TRadioButton
      Left = 100
      Top = 35
      Width = 90
      Height = 17
      Caption = 'configuration'
      TabOrder = 2
    end
    object RadioButton7: TRadioButton
      Left = 100
      Top = 55
      Width = 90
      Height = 17
      Caption = 'NVinfo'
      TabOrder = 3
    end
    object RadioButton8: TRadioButton
      Left = 200
      Top = 35
      Width = 46
      Height = 17
      Caption = 'light'
      TabOrder = 4
    end
    object RadioButton21: TRadioButton
      Left = 100
      Top = 75
      Width = 90
      Height = 17
      Caption = 'brief telemetry'
      TabOrder = 5
    end
    object RadioButton22: TRadioButton
      Left = 200
      Top = 55
      Width = 46
      Height = 17
      Caption = 'volt'
      TabOrder = 6
    end
    object RadioButton23: TRadioButton
      Left = 200
      Top = 75
      Width = 46
      Height = 17
      Caption = 'temp'
      TabOrder = 7
    end
    object RadioButton24: TRadioButton
      Left = 245
      Top = 35
      Width = 46
      Height = 17
      Caption = 'light'
      TabOrder = 8
    end
    object RadioButton25: TRadioButton
      Left = 245
      Top = 55
      Width = 46
      Height = 17
      Caption = 'volt'
      TabOrder = 9
    end
    object RadioButton26: TRadioButton
      Left = 245
      Top = 75
      Width = 46
      Height = 17
      Caption = 'temp'
      TabOrder = 10
    end
  end
  object GroupBox3: TGroupBox
    Left = 360
    Top = 240
    Width = 296
    Height = 56
    Caption = ' CW '
    TabOrder = 5
    object Button9: TButton
      Left = 10
      Top = 20
      Width = 75
      Height = 21
      Caption = 'CW'
      TabOrder = 0
      OnClick = CmdButtonClick
    end
    object RadioButton9: TRadioButton
      Left = 100
      Top = 15
      Width = 113
      Height = 17
      Caption = 'hello'
      Checked = True
      TabOrder = 1
      TabStop = True
    end
  end
  object GroupBox4: TGroupBox
    Left = 360
    Top = 305
    Width = 296
    Height = 201
    Caption = ' Service '
    TabOrder = 6
    object Button10: TButton
      Left = 10
      Top = 20
      Width = 75
      Height = 21
      Caption = 'Service'
      TabOrder = 0
      OnClick = CmdButtonClick
    end
    object RadioButton10: TRadioButton
      Left = 10
      Top = 50
      Width = 113
      Height = 17
      Caption = 'clear EEPROM log'
      TabOrder = 1
    end
    object RadioButton11: TRadioButton
      Left = 10
      Top = 70
      Width = 113
      Height = 17
      Caption = 'reboot MCU'
      TabOrder = 2
    end
    object RadioButton12: TRadioButton
      Left = 10
      Top = 90
      Width = 113
      Height = 17
      Caption = 'load config'
      TabOrder = 3
    end
    object RadioButton13: TRadioButton
      Left = 10
      Top = 110
      Width = 113
      Height = 17
      Caption = 'save config'
      TabOrder = 4
    end
    object RadioButton14: TRadioButton
      Left = 10
      Top = 130
      Width = 113
      Height = 17
      Caption = 'default config'
      TabOrder = 5
    end
    object RadioButton15: TRadioButton
      Left = 140
      Top = 110
      Width = 113
      Height = 17
      Caption = 'full flash erase'
      TabOrder = 6
    end
    object RadioButton16: TRadioButton
      Left = 140
      Top = 90
      Width = 113
      Height = 17
      Caption = 'full EEPROM erase'
      TabOrder = 7
    end
    object RadioButton17: TRadioButton
      Left = 140
      Top = 70
      Width = 113
      Height = 17
      Caption = 'reboot by fault'
      TabOrder = 8
    end
    object RadioButton18: TRadioButton
      Left = 140
      Top = 50
      Width = 113
      Height = 17
      Caption = 'reboot by WDT'
      TabOrder = 9
    end
    object RadioButton19: TRadioButton
      Left = 10
      Top = 150
      Width = 113
      Height = 17
      Caption = 'enable service'
      Checked = True
      TabOrder = 10
      TabStop = True
    end
    object RadioButton20: TRadioButton
      Left = 140
      Top = 130
      Width = 113
      Height = 17
      Caption = 'flash sector erase'
      TabOrder = 11
    end
    object Edit3: TEdit
      Left = 210
      Top = 147
      Width = 46
      Height = 21
      TabOrder = 12
      Text = '0'
    end
    object RadioButton27: TRadioButton
      Left = 10
      Top = 170
      Width = 113
      Height = 17
      Caption = 'disable service'
      TabOrder = 13
    end
  end
  object GroupBox5: TGroupBox
    Left = 360
    Top = 515
    Width = 296
    Height = 96
    Caption = ' Register access '
    TabOrder = 7
    object Label1: TLabel
      Left = 10
      Top = 20
      Width = 38
      Height = 13
      Caption = 'Address'
    end
    object Label2: TLabel
      Left = 100
      Top = 20
      Width = 27
      Height = 13
      Caption = 'Value'
    end
    object Label3: TLabel
      Left = 10
      Top = 60
      Width = 39
      Height = 13
      Caption = 'Label3'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -11
      Font.Name = 'MS Sans Serif'
      Font.Style = [fsBold]
      ParentFont = False
    end
    object Edit4: TEdit
      Left = 10
      Top = 35
      Width = 71
      Height = 21
      TabOrder = 0
      Text = '0'
      OnChange = Edit4Change
    end
    object Edit5: TEdit
      Left = 100
      Top = 35
      Width = 71
      Height = 21
      TabOrder = 1
      Text = '0'
    end
    object Button11: TButton
      Left = 205
      Top = 35
      Width = 75
      Height = 21
      Caption = 'Read'
      TabOrder = 2
      OnClick = Button11Click
    end
    object Button12: TButton
      Left = 205
      Top = 60
      Width = 75
      Height = 21
      Caption = 'Write'
      TabOrder = 3
      OnClick = Button12Click
    end
  end
  object Timer1: TTimer
    Interval = 500
    OnTimer = Timer1Timer
    Left = 555
    Top = 290
  end
end
