namespace WinFormsMitm
{
    partial class MainForm
    {
        private System.ComponentModel.IContainer components = null;

        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        private void InitializeComponent()
        {
            btnStart = new Button();
            txtLog = new RichTextBox();
            tabs = new TabControl();
            tabGeneral = new TabPage();
            groupBox1 = new GroupBox();
            txtRealHost = new TextBox();
            labelRealHost = new Label();
            txtListenPort = new TextBox();
            labelListenPort = new Label();
            txtExePath = new TextBox();
            btnBrowse = new Button();
            btnLaunch = new Button();
            lblStatus = new Label();
            label1 = new Label();
            tabPage1 = new TabPage();
            groupBox5 = new GroupBox();
            chkEnableAttack = new CheckBox();
            label5 = new Label();
            txtHitCount = new TextBox();
            label4 = new Label();
            txtDelay = new TextBox();
            label3 = new Label();
            txtRange = new TextBox();
            rbSkill = new RadioButton();
            rbNormalAttack = new RadioButton();
            cbSkills = new ComboBox();
            groupBoxPotion = new GroupBox();
            chkAutoPotion = new CheckBox();
            labelPotionHp = new Label();
            txtPotionHp = new TextBox();
            labelPotionMp = new Label();
            txtPotionMp = new TextBox();
            labelPotionInterval = new Label();
            txtPotionInterval = new TextBox();
            labelPotionHpItem = new Label();
            txtPotionHpItem = new TextBox();
            labelPotionMpItem = new Label();
            txtPotionMpItem = new TextBox();
            labelPotionHpSlot = new Label();
            txtPotionHpSlot = new TextBox();
            labelPotionMpSlot = new Label();
            txtPotionMpSlot = new TextBox();
            btnPotionApply = new Button();
            btnPotionStatus = new Button();
            statusStrip1 = new StatusStrip();
            toolTipPlayerCoord = new ToolStripStatusLabel();
            toolTipPlayerInfo = new ToolStripStatusLabel();
            groupBox4 = new GroupBox();
            chkEnableMonsterCapturing = new CheckBox();
            lstMonsters = new ListView();
            clmMonsterID = new ColumnHeader();
            clmMonsterName = new ColumnHeader();
            clmRange = new ColumnHeader();
            tabPackets = new TabPage();
            gbPacket = new GroupBox();
            groupBox3 = new GroupBox();
            label2 = new Label();
            txtTypeFilter = new TextBox();
            cbC4 = new CheckBox();
            cbC1 = new CheckBox();
            btnBypassPkt = new Button();
            cbC2 = new CheckBox();
            cbC3 = new CheckBox();
            btnClearPacket = new Button();
            cbOther = new CheckBox();
            cbAutoScroll = new CheckBox();
            groupBox2 = new GroupBox();
            txtPacket = new TextBox();
            rbServer = new RadioButton();
            rbClient = new RadioButton();
            btnSendPacketDecoded = new Button();
            lvPackets = new ListView();
            clmOrigin = new ColumnHeader();
            clmData = new ColumnHeader();
            clmLenght = new ColumnHeader();
            clmType = new ColumnHeader();
            clmPacket = new ColumnHeader();
            clmPlain = new ColumnHeader();
            tabLogs = new TabPage();
            btnClear = new Button();
            tabs.SuspendLayout();
            tabGeneral.SuspendLayout();
            groupBox1.SuspendLayout();
            tabPage1.SuspendLayout();
            groupBox5.SuspendLayout();
            groupBoxPotion.SuspendLayout();
            statusStrip1.SuspendLayout();
            groupBox4.SuspendLayout();
            tabPackets.SuspendLayout();
            gbPacket.SuspendLayout();
            groupBox3.SuspendLayout();
            groupBox2.SuspendLayout();
            tabLogs.SuspendLayout();
            SuspendLayout();
            // 
            // btnStart
            // 
            btnStart.Location = new Point(26, 47);
            btnStart.Margin = new Padding(2);
            btnStart.Name = "btnStart";
            btnStart.Size = new Size(150, 42);
            btnStart.TabIndex = 0;
            btnStart.Text = "Start Proxy";
            btnStart.UseVisualStyleBackColor = true;
            btnStart.Click += btnStart_Click;
            // 
            // txtLog
            // 
            txtLog.DetectUrls = false;
            txtLog.Dock = DockStyle.Fill;
            txtLog.Font = new Font("Consolas", 10F);
            txtLog.Location = new Point(2, 2);
            txtLog.Margin = new Padding(2);
            txtLog.Name = "txtLog";
            txtLog.ReadOnly = true;
            txtLog.Size = new Size(1760, 888);
            txtLog.TabIndex = 1;
            txtLog.Text = "";
            txtLog.WordWrap = false;
            // 
            // tabs
            // 
            tabs.Controls.Add(tabGeneral);
            tabs.Controls.Add(tabPage1);
            tabs.Controls.Add(tabPackets);
            tabs.Controls.Add(tabLogs);
            tabs.Dock = DockStyle.Fill;
            tabs.Location = new Point(0, 0);
            tabs.Margin = new Padding(2);
            tabs.Name = "tabs";
            tabs.SelectedIndex = 0;
            tabs.Size = new Size(1053, 715);
            tabs.TabIndex = 8;
            // 
            // tabGeneral
            // 
            tabGeneral.Controls.Add(groupBox1);
            tabGeneral.Location = new Point(4, 24);
            tabGeneral.Margin = new Padding(2);
            tabGeneral.Name = "tabGeneral";
            tabGeneral.Padding = new Padding(2);
            tabGeneral.Size = new Size(1764, 892);
            tabGeneral.TabIndex = 0;
            tabGeneral.Text = "General";
            tabGeneral.UseVisualStyleBackColor = true;
            // 
            // groupBox1
            // 
            groupBox1.Controls.Add(txtRealHost);
            groupBox1.Controls.Add(labelRealHost);
            groupBox1.Controls.Add(txtListenPort);
            groupBox1.Controls.Add(labelListenPort);
            groupBox1.Controls.Add(txtExePath);
            groupBox1.Controls.Add(btnBrowse);
            groupBox1.Controls.Add(btnLaunch);
            groupBox1.Controls.Add(lblStatus);
            groupBox1.Controls.Add(label1);
            groupBox1.Controls.Add(btnStart);
            groupBox1.Location = new Point(6, 4);
            groupBox1.Margin = new Padding(2);
            groupBox1.Name = "groupBox1";
            groupBox1.Padding = new Padding(2);
            groupBox1.Size = new Size(620, 170);
            groupBox1.TabIndex = 7;
            groupBox1.TabStop = false;
            groupBox1.Text = "Connection";
            // 
            // txtRealHost
            // 
            txtRealHost.Location = new Point(281, 128);
            txtRealHost.Margin = new Padding(2);
            txtRealHost.Name = "txtRealHost";
            txtRealHost.Size = new Size(160, 23);
            txtRealHost.TabIndex = 15;
            txtRealHost.Text = "145.239.149.84";
            // 
            // labelRealHost
            // 
            labelRealHost.AutoSize = true;
            labelRealHost.Location = new Point(199, 132);
            labelRealHost.Margin = new Padding(2, 0, 2, 0);
            labelRealHost.Name = "labelRealHost";
            labelRealHost.Size = new Size(67, 15);
            labelRealHost.TabIndex = 16;
            labelRealHost.Text = "Real Server:";
            // 
            // txtListenPort
            // 
            txtListenPort.Location = new Point(107, 128);
            txtListenPort.Margin = new Padding(2);
            txtListenPort.Name = "txtListenPort";
            txtListenPort.Size = new Size(70, 23);
            txtListenPort.TabIndex = 13;
            txtListenPort.Text = "44411";
            // 
            // labelListenPort
            // 
            labelListenPort.AutoSize = true;
            labelListenPort.Location = new Point(27, 132);
            labelListenPort.Margin = new Padding(2, 0, 2, 0);
            labelListenPort.Name = "labelListenPort";
            labelListenPort.Size = new Size(72, 15);
            labelListenPort.TabIndex = 14;
            labelListenPort.Text = "Srv List Port:";
            // 
            // txtExePath
            // 
            txtExePath.Location = new Point(26, 22);
            txtExePath.Margin = new Padding(2);
            txtExePath.Name = "txtExePath";
            txtExePath.Size = new Size(574, 23);
            txtExePath.TabIndex = 11;
            txtExePath.Text = "F:\\blackrockmu\\main.exe";
            // 
            // btnBrowse
            // 
            btnBrowse.Location = new Point(510, 47);
            btnBrowse.Margin = new Padding(2);
            btnBrowse.Name = "btnBrowse";
            btnBrowse.Size = new Size(90, 42);
            btnBrowse.TabIndex = 12;
            btnBrowse.Text = "Browse...";
            btnBrowse.UseVisualStyleBackColor = true;
            btnBrowse.Click += btnBrowse_Click;
            // 
            // btnLaunch
            // 
            btnLaunch.Location = new Point(356, 47);
            btnLaunch.Margin = new Padding(2);
            btnLaunch.Name = "btnLaunch";
            btnLaunch.Size = new Size(150, 42);
            btnLaunch.TabIndex = 9;
            btnLaunch.Text = "Launch Game";
            btnLaunch.UseVisualStyleBackColor = true;
            btnLaunch.Click += btnLaunch_Click;
            // 
            // lblStatus
            // 
            lblStatus.AutoSize = true;
            lblStatus.Location = new Point(27, 101);
            lblStatus.Margin = new Padding(2, 0, 2, 0);
            lblStatus.Name = "lblStatus";
            lblStatus.Size = new Size(103, 15);
            lblStatus.TabIndex = 1;
            lblStatus.Text = "Status: Connected";
            // 
            // label1
            // 
            label1.Location = new Point(0, 0);
            label1.Margin = new Padding(2, 0, 2, 0);
            label1.Name = "label1";
            label1.Size = new Size(70, 14);
            label1.TabIndex = 0;
            // 
            // tabPage1
            // 
            tabPage1.Controls.Add(groupBox5);
            tabPage1.Controls.Add(groupBoxPotion);
            tabPage1.Controls.Add(statusStrip1);
            tabPage1.Controls.Add(groupBox4);
            tabPage1.Location = new Point(4, 24);
            tabPage1.Name = "tabPage1";
            tabPage1.Padding = new Padding(3);
            tabPage1.Size = new Size(1045, 687);
            tabPage1.TabIndex = 3;
            tabPage1.Text = "Custom Stuff";
            tabPage1.UseVisualStyleBackColor = true;
            // 
            // groupBox5
            // 
            groupBox5.Controls.Add(chkEnableAttack);
            groupBox5.Controls.Add(label5);
            groupBox5.Controls.Add(txtHitCount);
            groupBox5.Controls.Add(label4);
            groupBox5.Controls.Add(txtDelay);
            groupBox5.Controls.Add(label3);
            groupBox5.Controls.Add(txtRange);
            groupBox5.Controls.Add(rbSkill);
            groupBox5.Controls.Add(rbNormalAttack);
            groupBox5.Controls.Add(cbSkills);
            groupBox5.Location = new Point(442, 7);
            groupBox5.Name = "groupBox5";
            groupBox5.Size = new Size(462, 252);
            groupBox5.TabIndex = 4;
            groupBox5.TabStop = false;
            groupBox5.Text = "Attack Config";
            // 
            // chkEnableAttack
            // 
            chkEnableAttack.AutoSize = true;
            chkEnableAttack.Location = new Point(11, 32);
            chkEnableAttack.Name = "chkEnableAttack";
            chkEnableAttack.Size = new Size(94, 19);
            chkEnableAttack.TabIndex = 2;
            chkEnableAttack.Text = "Enable Atack";
            chkEnableAttack.UseVisualStyleBackColor = true;
            // 
            // label5
            // 
            label5.AutoSize = true;
            label5.Location = new Point(11, 217);
            label5.Name = "label5";
            label5.Size = new Size(28, 15);
            label5.TabIndex = 8;
            label5.Text = "Hits";
            // 
            // txtHitCount
            // 
            txtHitCount.Location = new Point(57, 213);
            txtHitCount.Name = "txtHitCount";
            txtHitCount.Size = new Size(53, 23);
            txtHitCount.TabIndex = 9;
            txtHitCount.Text = "1";
            // 
            // label4
            // 
            label4.AutoSize = true;
            label4.Location = new Point(11, 177);
            label4.Name = "label4";
            label4.Size = new Size(36, 15);
            label4.TabIndex = 7;
            label4.Text = "Delay";
            // 
            // txtDelay
            // 
            txtDelay.Location = new Point(57, 173);
            txtDelay.Name = "txtDelay";
            txtDelay.Size = new Size(53, 23);
            txtDelay.TabIndex = 6;
            txtDelay.Text = "300";
            // 
            // label3
            // 
            label3.AutoSize = true;
            label3.Location = new Point(11, 137);
            label3.Name = "label3";
            label3.Size = new Size(40, 15);
            label3.TabIndex = 5;
            label3.Text = "Range";
            // 
            // txtRange
            // 
            txtRange.Location = new Point(57, 133);
            txtRange.Name = "txtRange";
            txtRange.Size = new Size(53, 23);
            txtRange.TabIndex = 4;
            txtRange.Text = "8";
            // 
            // rbSkill
            // 
            rbSkill.AutoSize = true;
            rbSkill.Location = new Point(111, 95);
            rbSkill.Name = "rbSkill";
            rbSkill.Size = new Size(46, 19);
            rbSkill.TabIndex = 3;
            rbSkill.TabStop = true;
            rbSkill.Text = "Skill";
            rbSkill.UseVisualStyleBackColor = true;
            // 
            // rbNormalAttack
            // 
            rbNormalAttack.AutoSize = true;
            rbNormalAttack.Location = new Point(10, 95);
            rbNormalAttack.Name = "rbNormalAttack";
            rbNormalAttack.Size = new Size(86, 19);
            rbNormalAttack.TabIndex = 2;
            rbNormalAttack.TabStop = true;
            rbNormalAttack.Text = "Normal Atk";
            rbNormalAttack.UseVisualStyleBackColor = true;
            // 
            // cbSkills
            // 
            cbSkills.FormattingEnabled = true;
            cbSkills.Location = new Point(163, 93);
            cbSkills.Name = "cbSkills";
            cbSkills.Size = new Size(280, 23);
            cbSkills.TabIndex = 0;
            // 
            // groupBoxPotion
            // 
            groupBoxPotion.Controls.Add(chkAutoPotion);
            groupBoxPotion.Controls.Add(labelPotionHp);
            groupBoxPotion.Controls.Add(txtPotionHp);
            groupBoxPotion.Controls.Add(labelPotionMp);
            groupBoxPotion.Controls.Add(txtPotionMp);
            groupBoxPotion.Controls.Add(labelPotionInterval);
            groupBoxPotion.Controls.Add(txtPotionInterval);
            groupBoxPotion.Controls.Add(labelPotionHpItem);
            groupBoxPotion.Controls.Add(txtPotionHpItem);
            groupBoxPotion.Controls.Add(labelPotionMpItem);
            groupBoxPotion.Controls.Add(txtPotionMpItem);
            groupBoxPotion.Controls.Add(labelPotionHpSlot);
            groupBoxPotion.Controls.Add(txtPotionHpSlot);
            groupBoxPotion.Controls.Add(labelPotionMpSlot);
            groupBoxPotion.Controls.Add(txtPotionMpSlot);
            groupBoxPotion.Controls.Add(btnPotionApply);
            groupBoxPotion.Controls.Add(btnPotionStatus);
            groupBoxPotion.Location = new Point(8, 265);
            groupBoxPotion.Name = "groupBoxPotion";
            groupBoxPotion.Size = new Size(425, 245);
            groupBoxPotion.TabIndex = 5;
            groupBoxPotion.TabStop = false;
            groupBoxPotion.Text = "Auto Potion";
            // 
            // chkAutoPotion
            // 
            chkAutoPotion.AutoSize = true;
            chkAutoPotion.Checked = true;
            chkAutoPotion.CheckState = CheckState.Checked;
            chkAutoPotion.Location = new Point(11, 26);
            chkAutoPotion.Name = "chkAutoPotion";
            chkAutoPotion.Size = new Size(140, 19);
            chkAutoPotion.TabIndex = 0;
            chkAutoPotion.Text = "Auto potion (HP/MP)";
            chkAutoPotion.UseVisualStyleBackColor = true;
            // 
            // labelPotionHp
            // 
            labelPotionHp.AutoSize = true;
            labelPotionHp.Location = new Point(11, 66);
            labelPotionHp.Name = "labelPotionHp";
            labelPotionHp.Size = new Size(90, 15);
            labelPotionHp.TabIndex = 1;
            labelPotionHp.Text = "HP abaixo de %";
            // 
            // txtPotionHp
            // 
            txtPotionHp.Location = new Point(105, 62);
            txtPotionHp.Name = "txtPotionHp";
            txtPotionHp.Size = new Size(53, 23);
            txtPotionHp.TabIndex = 2;
            txtPotionHp.Text = "50";
            // 
            // labelPotionMp
            // 
            labelPotionMp.AutoSize = true;
            labelPotionMp.Location = new Point(180, 66);
            labelPotionMp.Name = "labelPotionMp";
            labelPotionMp.Size = new Size(92, 15);
            labelPotionMp.TabIndex = 3;
            labelPotionMp.Text = "MP abaixo de %";
            // 
            // txtPotionMp
            // 
            txtPotionMp.Location = new Point(278, 62);
            txtPotionMp.Name = "txtPotionMp";
            txtPotionMp.Size = new Size(53, 23);
            txtPotionMp.TabIndex = 4;
            txtPotionMp.Text = "30";
            // 
            // labelPotionInterval
            // 
            labelPotionInterval.AutoSize = true;
            labelPotionInterval.Location = new Point(11, 106);
            labelPotionInterval.Name = "labelPotionInterval";
            labelPotionInterval.Size = new Size(80, 15);
            labelPotionInterval.TabIndex = 5;
            labelPotionInterval.Text = "Intervalo (ms)";
            // 
            // txtPotionInterval
            // 
            txtPotionInterval.Location = new Point(105, 102);
            txtPotionInterval.Name = "txtPotionInterval";
            txtPotionInterval.Size = new Size(53, 23);
            txtPotionInterval.TabIndex = 6;
            txtPotionInterval.Text = "1000";
            //
            // labelPotionHpItem
            //
            labelPotionHpItem.AutoSize = true;
            labelPotionHpItem.Location = new Point(11, 146);
            labelPotionHpItem.Name = "labelPotionHpItem";
            labelPotionHpItem.Size = new Size(118, 15);
            labelPotionHpItem.TabIndex = 7;
            labelPotionHpItem.Text = "Item de HP (hex)";
            //
            // txtPotionHpItem
            //
            txtPotionHpItem.Location = new Point(133, 142);
            txtPotionHpItem.Name = "txtPotionHpItem";
            txtPotionHpItem.Size = new Size(60, 23);
            txtPotionHpItem.TabIndex = 8;
            txtPotionHpItem.PlaceholderText = "0x0C";
            txtPotionHpItem.Text = "";
            //
            // labelPotionMpItem
            //
            labelPotionMpItem.AutoSize = true;
            labelPotionMpItem.Location = new Point(210, 146);
            labelPotionMpItem.Name = "labelPotionMpItem";
            labelPotionMpItem.Size = new Size(118, 15);
            labelPotionMpItem.TabIndex = 9;
            labelPotionMpItem.Text = "Item de MP (hex)";
            //
            // txtPotionMpItem
            //
            txtPotionMpItem.Location = new Point(332, 142);
            txtPotionMpItem.Name = "txtPotionMpItem";
            txtPotionMpItem.Size = new Size(60, 23);
            txtPotionMpItem.TabIndex = 10;
            txtPotionMpItem.PlaceholderText = "0x1C05";
            txtPotionMpItem.Text = "";
            //
            // labelPotionHpSlot
            //
            labelPotionHpSlot.AutoSize = true;
            labelPotionHpSlot.Location = new Point(11, 186);
            labelPotionHpSlot.Name = "labelPotionHpSlot";
            labelPotionHpSlot.Size = new Size(118, 15);
            labelPotionHpSlot.TabIndex = 11;
            labelPotionHpSlot.Text = "Slot de HP";
            //
            // txtPotionHpSlot
            //
            txtPotionHpSlot.Location = new Point(133, 182);
            txtPotionHpSlot.Name = "txtPotionHpSlot";
            txtPotionHpSlot.Size = new Size(60, 23);
            txtPotionHpSlot.TabIndex = 12;
            txtPotionHpSlot.PlaceholderText = "auto";
            txtPotionHpSlot.Text = "";
            //
            // labelPotionMpSlot
            //
            labelPotionMpSlot.AutoSize = true;
            labelPotionMpSlot.Location = new Point(210, 186);
            labelPotionMpSlot.Name = "labelPotionMpSlot";
            labelPotionMpSlot.Size = new Size(118, 15);
            labelPotionMpSlot.TabIndex = 13;
            labelPotionMpSlot.Text = "Slot de MP";
            //
            // txtPotionMpSlot
            //
            txtPotionMpSlot.Location = new Point(332, 182);
            txtPotionMpSlot.Name = "txtPotionMpSlot";
            txtPotionMpSlot.Size = new Size(60, 23);
            txtPotionMpSlot.TabIndex = 14;
            txtPotionMpSlot.PlaceholderText = "auto";
            txtPotionMpSlot.Text = "";
            // 
            // btnPotionApply
            // 
            btnPotionApply.Location = new Point(180, 101);
            btnPotionApply.Name = "btnPotionApply";
            btnPotionApply.Size = new Size(80, 25);
            btnPotionApply.TabIndex = 7;
            btnPotionApply.Text = "Aplicar";
            btnPotionApply.UseVisualStyleBackColor = true;
            // 
            // btnPotionStatus
            // 
            btnPotionStatus.Location = new Point(270, 101);
            btnPotionStatus.Name = "btnPotionStatus";
            btnPotionStatus.Size = new Size(80, 25);
            btnPotionStatus.TabIndex = 8;
            btnPotionStatus.Text = "Status";
            btnPotionStatus.UseVisualStyleBackColor = true;
            // 
            // statusStrip1
            // 
            statusStrip1.Items.AddRange(new ToolStripItem[] { toolTipPlayerCoord, toolTipPlayerInfo });
            statusStrip1.Location = new Point(3, 662);
            statusStrip1.Name = "statusStrip1";
            statusStrip1.Size = new Size(1039, 22);
            statusStrip1.TabIndex = 3;
            statusStrip1.Text = "statusStrip1";
            // 
            // toolTipPlayerCoord
            // 
            toolTipPlayerCoord.Name = "toolTipPlayerCoord";
            toolTipPlayerCoord.Size = new Size(35, 17);
            toolTipPlayerCoord.Text = "[X, Y]";
            // 
            // toolTipPlayerInfo
            // 
            toolTipPlayerInfo.Name = "toolTipPlayerInfo";
            toolTipPlayerInfo.Size = new Size(989, 17);
            toolTipPlayerInfo.Spring = true;
            toolTipPlayerInfo.Text = "Idx:0x0000 | Pos:(0,0) | Range:0 | Entities:0 | Monsters:0";
            // 
            // groupBox4
            // 
            groupBox4.Controls.Add(chkEnableMonsterCapturing);
            groupBox4.Controls.Add(lstMonsters);
            groupBox4.Location = new Point(7, 7);
            groupBox4.Name = "groupBox4";
            groupBox4.Size = new Size(429, 252);
            groupBox4.TabIndex = 1;
            groupBox4.TabStop = false;
            groupBox4.Text = "Monster List";
            // 
            // chkEnableMonsterCapturing
            // 
            chkEnableMonsterCapturing.AutoSize = true;
            chkEnableMonsterCapturing.Checked = true;
            chkEnableMonsterCapturing.CheckState = CheckState.Checked;
            chkEnableMonsterCapturing.Location = new Point(9, 20);
            chkEnableMonsterCapturing.Name = "chkEnableMonsterCapturing";
            chkEnableMonsterCapturing.Size = new Size(164, 19);
            chkEnableMonsterCapturing.TabIndex = 1;
            chkEnableMonsterCapturing.Text = "Enable Monster Capturing";
            chkEnableMonsterCapturing.UseVisualStyleBackColor = true;
            // 
            // lstMonsters
            // 
            lstMonsters.Columns.AddRange(new ColumnHeader[] { clmMonsterID, clmMonsterName, clmRange });
            lstMonsters.GridLines = true;
            lstMonsters.Location = new Point(3, 45);
            lstMonsters.Name = "lstMonsters";
            lstMonsters.Size = new Size(423, 201);
            lstMonsters.TabIndex = 0;
            lstMonsters.UseCompatibleStateImageBehavior = false;
            lstMonsters.View = View.Details;
            // 
            // clmMonsterID
            // 
            clmMonsterID.Text = "Monster Id";
            clmMonsterID.Width = 150;
            // 
            // clmMonsterName
            // 
            clmMonsterName.Text = "Monster Name";
            clmMonsterName.Width = 200;
            // 
            // clmRange
            // 
            clmRange.Text = "Range";
            // 
            // tabPackets
            // 
            tabPackets.Controls.Add(gbPacket);
            tabPackets.Controls.Add(lvPackets);
            tabPackets.Location = new Point(4, 24);
            tabPackets.Margin = new Padding(2);
            tabPackets.Name = "tabPackets";
            tabPackets.Padding = new Padding(2);
            tabPackets.Size = new Size(1045, 687);
            tabPackets.TabIndex = 2;
            tabPackets.Text = "Packets";
            tabPackets.UseVisualStyleBackColor = true;
            // 
            // gbPacket
            // 
            gbPacket.Controls.Add(groupBox3);
            gbPacket.Controls.Add(groupBox2);
            gbPacket.Dock = DockStyle.Top;
            gbPacket.Location = new Point(2, 2);
            gbPacket.Margin = new Padding(2);
            gbPacket.Name = "gbPacket";
            gbPacket.Padding = new Padding(2);
            gbPacket.Size = new Size(1041, 114);
            gbPacket.TabIndex = 1;
            gbPacket.TabStop = false;
            gbPacket.Text = "Packet Management";
            // 
            // groupBox3
            // 
            groupBox3.Controls.Add(label2);
            groupBox3.Controls.Add(txtTypeFilter);
            groupBox3.Controls.Add(cbC4);
            groupBox3.Controls.Add(cbC1);
            groupBox3.Controls.Add(btnBypassPkt);
            groupBox3.Controls.Add(cbC2);
            groupBox3.Controls.Add(cbC3);
            groupBox3.Controls.Add(btnClearPacket);
            groupBox3.Controls.Add(cbOther);
            groupBox3.Controls.Add(cbAutoScroll);
            groupBox3.Location = new Point(6, 21);
            groupBox3.Name = "groupBox3";
            groupBox3.Size = new Size(586, 86);
            groupBox3.TabIndex = 10;
            groupBox3.TabStop = false;
            groupBox3.Text = "Protocol Filter";
            // 
            // label2
            // 
            label2.AutoSize = true;
            label2.Location = new Point(356, 32);
            label2.Name = "label2";
            label2.Size = new Size(207, 15);
            label2.TabIndex = 12;
            label2.Text = "Packet Type: ALL | UNKNOWN | Name";
            // 
            // txtTypeFilter
            // 
            txtTypeFilter.Location = new Point(341, 51);
            txtTypeFilter.Name = "txtTypeFilter";
            txtTypeFilter.Size = new Size(239, 23);
            txtTypeFilter.TabIndex = 11;
            // 
            // cbC4
            // 
            cbC4.AutoSize = true;
            cbC4.Checked = true;
            cbC4.CheckState = CheckState.Checked;
            cbC4.Location = new Point(47, 46);
            cbC4.Margin = new Padding(2);
            cbC4.Name = "cbC4";
            cbC4.Size = new Size(40, 19);
            cbC4.TabIndex = 7;
            cbC4.Text = "C4";
            cbC4.UseVisualStyleBackColor = true;
            cbC4.CheckedChanged += cbC4_CheckedChanged;
            // 
            // cbC1
            // 
            cbC1.AutoSize = true;
            cbC1.Checked = true;
            cbC1.CheckState = CheckState.Checked;
            cbC1.Location = new Point(3, 21);
            cbC1.Margin = new Padding(2);
            cbC1.Name = "cbC1";
            cbC1.Size = new Size(40, 19);
            cbC1.TabIndex = 4;
            cbC1.Text = "C1";
            cbC1.UseVisualStyleBackColor = true;
            // 
            // btnBypassPkt
            // 
            btnBypassPkt.Location = new Point(235, 12);
            btnBypassPkt.Margin = new Padding(2);
            btnBypassPkt.Name = "btnBypassPkt";
            btnBypassPkt.Size = new Size(82, 27);
            btnBypassPkt.TabIndex = 8;
            btnBypassPkt.Text = "Bypass Pkt";
            btnBypassPkt.UseVisualStyleBackColor = true;
            btnBypassPkt.Click += btnBypassPkt_Click;
            // 
            // cbC2
            // 
            cbC2.AutoSize = true;
            cbC2.Checked = true;
            cbC2.CheckState = CheckState.Checked;
            cbC2.Location = new Point(47, 21);
            cbC2.Margin = new Padding(2);
            cbC2.Name = "cbC2";
            cbC2.Size = new Size(40, 19);
            cbC2.TabIndex = 5;
            cbC2.Text = "C2";
            cbC2.UseVisualStyleBackColor = true;
            // 
            // cbC3
            // 
            cbC3.AutoSize = true;
            cbC3.Checked = true;
            cbC3.CheckState = CheckState.Checked;
            cbC3.Location = new Point(3, 46);
            cbC3.Margin = new Padding(2);
            cbC3.Name = "cbC3";
            cbC3.Size = new Size(40, 19);
            cbC3.TabIndex = 6;
            cbC3.Text = "C3";
            cbC3.UseVisualStyleBackColor = true;
            // 
            // btnClearPacket
            // 
            btnClearPacket.Location = new Point(235, 46);
            btnClearPacket.Margin = new Padding(2);
            btnClearPacket.Name = "btnClearPacket";
            btnClearPacket.Size = new Size(82, 28);
            btnClearPacket.TabIndex = 2;
            btnClearPacket.Text = "Clear";
            btnClearPacket.UseVisualStyleBackColor = true;
            btnClearPacket.Click += btnClearPacket_Click;
            // 
            // cbOther
            // 
            cbOther.AutoSize = true;
            cbOther.Checked = true;
            cbOther.CheckState = CheckState.Checked;
            cbOther.Location = new Point(116, 21);
            cbOther.Margin = new Padding(2);
            cbOther.Name = "cbOther";
            cbOther.Size = new Size(62, 19);
            cbOther.TabIndex = 8;
            cbOther.Text = "Outros";
            cbOther.UseVisualStyleBackColor = true;
            // 
            // cbAutoScroll
            // 
            cbAutoScroll.AutoSize = true;
            cbAutoScroll.Location = new Point(116, 46);
            cbAutoScroll.Margin = new Padding(2);
            cbAutoScroll.Name = "cbAutoScroll";
            cbAutoScroll.Size = new Size(84, 19);
            cbAutoScroll.TabIndex = 4;
            cbAutoScroll.Text = "Auto Scroll";
            cbAutoScroll.UseVisualStyleBackColor = true;
            // 
            // groupBox2
            // 
            groupBox2.Controls.Add(txtPacket);
            groupBox2.Controls.Add(rbServer);
            groupBox2.Controls.Add(rbClient);
            groupBox2.Controls.Add(btnSendPacketDecoded);
            groupBox2.Location = new Point(622, 21);
            groupBox2.Name = "groupBox2";
            groupBox2.Size = new Size(633, 86);
            groupBox2.TabIndex = 9;
            groupBox2.TabStop = false;
            groupBox2.Text = "Packet Sender";
            // 
            // txtPacket
            // 
            txtPacket.Location = new Point(5, 54);
            txtPacket.Margin = new Padding(2);
            txtPacket.Name = "txtPacket";
            txtPacket.Size = new Size(623, 23);
            txtPacket.TabIndex = 2;
            // 
            // rbServer
            // 
            rbServer.AutoSize = true;
            rbServer.Location = new Point(65, 21);
            rbServer.Margin = new Padding(2);
            rbServer.Name = "rbServer";
            rbServer.Size = new Size(57, 19);
            rbServer.TabIndex = 1;
            rbServer.TabStop = true;
            rbServer.Text = "Server";
            rbServer.UseVisualStyleBackColor = true;
            // 
            // rbClient
            // 
            rbClient.AutoSize = true;
            rbClient.Location = new Point(5, 21);
            rbClient.Margin = new Padding(2);
            rbClient.Name = "rbClient";
            rbClient.Size = new Size(56, 19);
            rbClient.TabIndex = 0;
            rbClient.TabStop = true;
            rbClient.Text = "Client";
            rbClient.UseVisualStyleBackColor = true;
            // 
            // btnSendPacketDecoded
            // 
            btnSendPacketDecoded.Location = new Point(483, 15);
            btnSendPacketDecoded.Margin = new Padding(2);
            btnSendPacketDecoded.Name = "btnSendPacketDecoded";
            btnSendPacketDecoded.Size = new Size(145, 32);
            btnSendPacketDecoded.TabIndex = 5;
            btnSendPacketDecoded.Text = "Send Packet";
            btnSendPacketDecoded.UseVisualStyleBackColor = true;
            btnSendPacketDecoded.Click += btnSendPacketDecoded_Click;
            // 
            // lvPackets
            // 
            lvPackets.Alignment = ListViewAlignment.SnapToGrid;
            lvPackets.BorderStyle = BorderStyle.FixedSingle;
            lvPackets.Columns.AddRange(new ColumnHeader[] { clmOrigin, clmData, clmLenght, clmType, clmPacket, clmPlain });
            lvPackets.Dock = DockStyle.Bottom;
            lvPackets.Font = new Font("Consolas", 10F, FontStyle.Bold);
            lvPackets.FullRowSelect = true;
            lvPackets.GridLines = true;
            lvPackets.Location = new Point(2, -85);
            lvPackets.Margin = new Padding(2);
            lvPackets.Name = "lvPackets";
            lvPackets.Size = new Size(1041, 770);
            lvPackets.TabIndex = 3;
            lvPackets.UseCompatibleStateImageBehavior = false;
            lvPackets.View = View.Details;
            lvPackets.DoubleClick += lvPackets_DoubleClick;
            lvPackets.KeyDown += lvPackets_KeyDown;
            // 
            // clmOrigin
            // 
            clmOrigin.Text = "Origin";
            clmOrigin.Width = 80;
            // 
            // clmData
            // 
            clmData.Text = "Data";
            clmData.Width = 120;
            // 
            // clmLenght
            // 
            clmLenght.Text = "Length";
            clmLenght.Width = 80;
            // 
            // clmType
            // 
            clmType.Text = "Type";
            clmType.Width = 120;
            // 
            // clmPacket
            // 
            clmPacket.Text = "Packet";
            clmPacket.Width = 600;
            // 
            // clmPlain
            // 
            clmPlain.Text = "Plain Text";
            clmPlain.Width = 800;
            // 
            // tabLogs
            // 
            tabLogs.Controls.Add(btnClear);
            tabLogs.Controls.Add(txtLog);
            tabLogs.Location = new Point(4, 24);
            tabLogs.Margin = new Padding(2);
            tabLogs.Name = "tabLogs";
            tabLogs.Padding = new Padding(2);
            tabLogs.Size = new Size(1764, 892);
            tabLogs.TabIndex = 1;
            tabLogs.Text = "Logs";
            tabLogs.UseVisualStyleBackColor = true;
            // 
            // btnClear
            // 
            btnClear.Location = new Point(4, 4);
            btnClear.Margin = new Padding(2);
            btnClear.Name = "btnClear";
            btnClear.Size = new Size(78, 20);
            btnClear.TabIndex = 2;
            btnClear.Text = "Clear";
            btnClear.UseVisualStyleBackColor = true;
            btnClear.Click += btnClear_Click;
            // 
            // MainForm
            // 
            AutoScaleDimensions = new SizeF(7F, 15F);
            AutoScaleMode = AutoScaleMode.Font;
            ClientSize = new Size(1053, 715);
            Controls.Add(tabs);
            Margin = new Padding(2);
            Name = "MainForm";
            Text = "MuProxy2 - Direct Hook";
            tabs.ResumeLayout(false);
            tabGeneral.ResumeLayout(false);
            groupBox1.ResumeLayout(false);
            groupBox1.PerformLayout();
            tabPage1.ResumeLayout(false);
            tabPage1.PerformLayout();
            groupBox5.ResumeLayout(false);
            groupBox5.PerformLayout();
            groupBoxPotion.ResumeLayout(false);
            groupBoxPotion.PerformLayout();
            statusStrip1.ResumeLayout(false);
            statusStrip1.PerformLayout();
            groupBox4.ResumeLayout(false);
            groupBox4.PerformLayout();
            tabPackets.ResumeLayout(false);
            gbPacket.ResumeLayout(false);
            groupBox3.ResumeLayout(false);
            groupBox3.PerformLayout();
            groupBox2.ResumeLayout(false);
            groupBox2.PerformLayout();
            tabLogs.ResumeLayout(false);
            ResumeLayout(false);
        }

        #endregion

        private Button btnStart;
        private RichTextBox txtLog;
        private TabControl tabs;
        private TabPage tabGeneral;
        private TabPage tabLogs;
        private GroupBox groupBox1;
        private Label label1;
        private Button btnClear;
        private TabPage tabPackets;
        private Label lblStatus;
        private GroupBox gbPacket;
        private TextBox txtPacket;
        private RadioButton rbServer;
        private RadioButton rbClient;
        private Button btnClearPacket;
        private ListView lvPackets;
        private ColumnHeader clmOrigin;
        private ColumnHeader clmData;
        private ColumnHeader clmLenght;
        private ColumnHeader clmType;
        private ColumnHeader clmPacket;
        private CheckBox cbAutoScroll;
        private CheckBox cbC4;
        private CheckBox cbC3;
        private CheckBox cbC2;
        private CheckBox cbC1;
        private CheckBox cbOther;
        private ColumnHeader clmPlain;
        private Button btnSendPacketDecoded;
        private Button btnBypassPkt;
        private Button btnLaunch;
        private TextBox txtExePath;
        private Button btnBrowse;
        private TextBox txtListenPort;
        private Label labelListenPort;
        private TextBox txtRealHost;
        private Label labelRealHost;
        private GroupBox groupBox2;
        private GroupBox groupBox3;
        private Label label2;
        private TextBox txtTypeFilter;
        private TabPage tabPage1;
        private GroupBox groupBox4;
        private ListView lstMonsters;
        private ColumnHeader clmMonsterID;
        private ColumnHeader clmMonsterName;
        private ColumnHeader clmRange;
        private StatusStrip statusStrip1;
        private ToolStripStatusLabel toolTipPlayerCoord;
        private CheckBox chkEnableMonsterCapturing;
        private GroupBox groupBox5;
        private RadioButton rbSkill;
        private RadioButton rbNormalAttack;
        private ComboBox cbSkills;
        private Label label4;
        private TextBox txtDelay;
        private Label label5;
        private TextBox txtHitCount;
        private Label label3;
        private TextBox txtRange;
        private CheckBox chkEnableAttack;
        private ToolStripStatusLabel toolTipPlayerInfo;
        private GroupBox groupBoxPotion;
        private CheckBox chkAutoPotion;
        private Label labelPotionHp;
        private TextBox txtPotionHp;
        private Label labelPotionMp;
        private TextBox txtPotionMp;
        private Label labelPotionInterval;
        private TextBox txtPotionInterval;
        private Label labelPotionHpItem;
        private TextBox txtPotionHpItem;
        private Label labelPotionMpItem;
        private TextBox txtPotionMpItem;
        private Label labelPotionHpSlot;
        private TextBox txtPotionHpSlot;
        private Label labelPotionMpSlot;
        private TextBox txtPotionMpSlot;
        private Button btnPotionApply;
        private Button btnPotionStatus;
    }
}
