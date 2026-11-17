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
            headerPanel = new Panel();
            lblSword = new Label();
            lblTitle = new Label();
            lblSubtitle = new Label();
            lblSeason = new Label();
            lblVersion = new Label();
            headerLine = new Panel();
            tabs = new TabControl();
            tabGeneral = new TabPage();
            groupBox1 = new GroupBox();
            lblExePath = new Label();
            txtExePath = new TextBox();
            btnBrowse = new Button();
            btnLaunch = new Button();
            btnStart = new Button();
            lblStatus = new Label();
            labelRealHost = new Label();
            txtRealHost = new TextBox();
            labelListenPort = new Label();
            txtListenPort = new TextBox();
            bannerPanel = new Panel();
            lblBannerMu = new Label();
            lblBannerName = new Label();
            lblBannerSub = new Label();
            bannerLine = new Panel();
            lblBannerFoot = new Label();
            infoPanel = new Panel();
            lblInfoTitle = new Label();
            lblInfo1 = new Label();
            lblInfo2 = new Label();
            lblInfo3 = new Label();
            lblInfo4 = new Label();
            tabPage1 = new TabPage();
            groupBox4 = new GroupBox();
            chkEnableMonsterCapturing = new CheckBox();
            lstMonsters = new ListView();
            clmMonsterID = new ColumnHeader();
            clmMonsterName = new ColumnHeader();
            clmMonsterType = new ColumnHeader();
            clmLevel = new ColumnHeader();
            clmRange = new ColumnHeader();
            clmAttackId = new ColumnHeader();
            clmPos = new ColumnHeader();
            groupBoxPotion = new GroupBox();
            chkAutoPotion = new CheckBox();
            labelPotionHp = new Label();
            txtPotionHp = new TextBox();
            labelPotionMp = new Label();
            txtPotionMp = new TextBox();
            labelPotionInterval = new Label();
            txtPotionInterval = new TextBox();
            lblPotionState = new Label();
            groupBox5 = new GroupBox();
            chkEnableAttack = new CheckBox();
            chkAttackMonsters = new CheckBox();
            chkAttackPlayerAll = new CheckBox();
            chkAttackPlayerClick = new CheckBox();
            chkAttackOthers = new CheckBox();
            rbNormalAttack = new RadioButton();
            rbSkill = new RadioButton();
            cbSkills = new ComboBox();
            label3 = new Label();
            txtRange = new TextBox();
            label4 = new Label();
            txtDelay = new TextBox();
            label5 = new Label();
            txtHitCount = new TextBox();
            lblAttackHint = new Label();
            groupBoxAccount = new GroupBox();
            lblAccount = new Label();
            txtAccount = new TextBox();
            lblPassword = new Label();
            txtPassword = new TextBox();
            btnAccountApply = new Button();
            lblAccountInfo = new Label();
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
            txtLog = new RichTextBox();
            logsTop = new Panel();
            btnClear = new Button();
            statusStrip1 = new StatusStrip();
            toolTipPlayerCoord = new ToolStripStatusLabel();
            toolTipPlayerInfo = new ToolStripStatusLabel();
            headerPanel.SuspendLayout();
            tabs.SuspendLayout();
            tabGeneral.SuspendLayout();
            groupBox1.SuspendLayout();
            bannerPanel.SuspendLayout();
            infoPanel.SuspendLayout();
            tabPage1.SuspendLayout();
            groupBox4.SuspendLayout();
            groupBoxPotion.SuspendLayout();
            groupBox5.SuspendLayout();
            groupBoxAccount.SuspendLayout();
            tabPackets.SuspendLayout();
            gbPacket.SuspendLayout();
            groupBox3.SuspendLayout();
            groupBox2.SuspendLayout();
            tabLogs.SuspendLayout();
            logsTop.SuspendLayout();
            statusStrip1.SuspendLayout();
            SuspendLayout();
            // 
            // headerPanel
            // 
            headerPanel.BackColor = Color.FromArgb(24, 27, 34);
            headerPanel.Controls.Add(lblSword);
            headerPanel.Controls.Add(lblTitle);
            headerPanel.Controls.Add(lblSubtitle);
            headerPanel.Controls.Add(lblSeason);
            headerPanel.Controls.Add(lblVersion);
            headerPanel.Controls.Add(headerLine);
            headerPanel.Dock = DockStyle.Top;
            headerPanel.Location = new Point(0, 0);
            headerPanel.Name = "headerPanel";
            headerPanel.Size = new Size(1280, 64);
            headerPanel.TabIndex = 1;
            headerPanel.Tag = "header";
            // 
            // lblSword
            // 
            lblSword.AutoSize = true;
            lblSword.BackColor = Color.Transparent;
            lblSword.Font = new Font("Segoe UI Symbol", 24F, FontStyle.Bold);
            lblSword.ForeColor = Color.FromArgb(227, 179, 65);
            lblSword.Location = new Point(16, 8);
            lblSword.Name = "lblSword";
            lblSword.Size = new Size(45, 45);
            lblSword.TabIndex = 0;
            lblSword.Tag = "keep";
            lblSword.Text = "⚔";
            // 
            // lblTitle
            // 
            lblTitle.AutoSize = true;
            lblTitle.BackColor = Color.Transparent;
            lblTitle.Font = new Font("Segoe UI", 20F, FontStyle.Bold);
            lblTitle.ForeColor = Color.FromArgb(232, 233, 235);
            lblTitle.Location = new Point(56, 6);
            lblTitle.Name = "lblTitle";
            lblTitle.Size = new Size(149, 37);
            lblTitle.TabIndex = 1;
            lblTitle.Tag = "keep";
            lblTitle.Text = "MuProxy2";
            // 
            // lblSubtitle
            // 
            lblSubtitle.AutoSize = true;
            lblSubtitle.BackColor = Color.Transparent;
            lblSubtitle.Font = new Font("Segoe UI", 8F);
            lblSubtitle.ForeColor = Color.FromArgb(178, 184, 196);
            lblSubtitle.Location = new Point(922, 25);
            lblSubtitle.Name = "lblSubtitle";
            lblSubtitle.Size = new Size(330, 13);
            lblSubtitle.TabIndex = 2;
            lblSubtitle.Tag = "keep";
            lblSubtitle.Text = "B L A C K R O C K   M U   ·   D I R E C T   H O O K   ·   BY: EVERTON";
            // 
            // lblSeason
            // 
            lblSeason.Anchor = AnchorStyles.Top | AnchorStyles.Right;
            lblSeason.AutoSize = true;
            lblSeason.BackColor = Color.Transparent;
            lblSeason.Font = new Font("Segoe UI", 10F, FontStyle.Bold);
            lblSeason.ForeColor = Color.FromArgb(227, 179, 65);
            lblSeason.Location = new Point(2200, 10);
            lblSeason.Name = "lblSeason";
            lblSeason.Size = new Size(76, 19);
            lblSeason.TabIndex = 3;
            lblSeason.Tag = "keep";
            lblSeason.Text = "SEASON 6";
            // 
            // lblVersion
            // 
            lblVersion.Anchor = AnchorStyles.Top | AnchorStyles.Right;
            lblVersion.AutoSize = true;
            lblVersion.BackColor = Color.Transparent;
            lblVersion.Font = new Font("Segoe UI", 9F);
            lblVersion.ForeColor = Color.FromArgb(178, 184, 196);
            lblVersion.Location = new Point(2200, 32);
            lblVersion.Name = "lblVersion";
            lblVersion.Size = new Size(37, 15);
            lblVersion.TabIndex = 4;
            lblVersion.Tag = "keep";
            lblVersion.Text = "v3.1.0";
            // 
            // headerLine
            // 
            headerLine.BackColor = Color.FromArgb(138, 109, 43);
            headerLine.Dock = DockStyle.Bottom;
            headerLine.Location = new Point(0, 62);
            headerLine.Margin = new Padding(0);
            headerLine.Name = "headerLine";
            headerLine.Size = new Size(1280, 2);
            headerLine.TabIndex = 5;
            headerLine.Tag = "keep";
            // 
            // tabs
            // 
            tabs.Controls.Add(tabGeneral);
            tabs.Controls.Add(tabPage1);
            tabs.Controls.Add(tabPackets);
            tabs.Controls.Add(tabLogs);
            tabs.Dock = DockStyle.Fill;
            tabs.Font = new Font("Segoe UI", 9F);
            tabs.ItemSize = new Size(150, 36);
            tabs.Location = new Point(0, 64);
            tabs.Margin = new Padding(0);
            tabs.Name = "tabs";
            tabs.SelectedIndex = 0;
            tabs.Size = new Size(1280, 714);
            tabs.SizeMode = TabSizeMode.Fixed;
            tabs.TabIndex = 0;
            // 
            // tabGeneral
            // 
            tabGeneral.BackColor = Color.FromArgb(15, 17, 21);
            tabGeneral.BackgroundImageLayout = ImageLayout.None;
            tabGeneral.Controls.Add(groupBox1);
            tabGeneral.Controls.Add(bannerPanel);
            tabGeneral.Controls.Add(infoPanel);
            tabGeneral.Cursor = Cursors.IBeam;
            tabGeneral.Font = new Font("Segoe UI", 9F);
            tabGeneral.ForeColor = Color.FromArgb(232, 233, 235);
            tabGeneral.Location = new Point(4, 40);
            tabGeneral.Name = "tabGeneral";
            tabGeneral.Size = new Size(1272, 670);
            tabGeneral.TabIndex = 0;
            tabGeneral.Text = "General";
            // 
            // groupBox1
            // 
            groupBox1.BackColor = Color.FromArgb(24, 27, 34);
            groupBox1.Controls.Add(lblExePath);
            groupBox1.Controls.Add(txtExePath);
            groupBox1.Controls.Add(btnBrowse);
            groupBox1.Controls.Add(btnLaunch);
            groupBox1.Controls.Add(btnStart);
            groupBox1.Controls.Add(lblStatus);
            groupBox1.Controls.Add(labelRealHost);
            groupBox1.Controls.Add(txtRealHost);
            groupBox1.Controls.Add(labelListenPort);
            groupBox1.Controls.Add(txtListenPort);
            groupBox1.Font = new Font("Segoe UI", 9.5F, FontStyle.Bold);
            groupBox1.ForeColor = Color.FromArgb(227, 179, 65);
            groupBox1.Location = new Point(16, 16);
            groupBox1.Name = "groupBox1";
            groupBox1.Size = new Size(600, 252);
            groupBox1.TabIndex = 0;
            groupBox1.TabStop = false;
            groupBox1.Text = "Connection";
            // 
            // lblExePath
            // 
            lblExePath.AutoSize = true;
            lblExePath.BackColor = Color.Transparent;
            lblExePath.Font = new Font("Segoe UI", 9F);
            lblExePath.ForeColor = Color.FromArgb(178, 184, 196);
            lblExePath.Location = new Point(16, 26);
            lblExePath.Name = "lblExePath";
            lblExePath.Size = new Size(110, 15);
            lblExePath.TabIndex = 0;
            lblExePath.Tag = "muted";
            lblExePath.Text = "GAME EXECUTABLE";
            // 
            // txtExePath
            // 
            txtExePath.BackColor = Color.FromArgb(19, 21, 27);
            txtExePath.BorderStyle = BorderStyle.FixedSingle;
            txtExePath.Font = new Font("Segoe UI", 9F);
            txtExePath.ForeColor = Color.FromArgb(232, 233, 235);
            txtExePath.Location = new Point(16, 46);
            txtExePath.Name = "txtExePath";
            txtExePath.Size = new Size(440, 23);
            txtExePath.TabIndex = 0;
            txtExePath.Text = "F:\\blackrockmu\\main.exe";
            // 
            // btnBrowse
            // 
            btnBrowse.BackColor = Color.FromArgb(31, 35, 45);
            btnBrowse.Cursor = Cursors.Hand;
            btnBrowse.FlatAppearance.BorderColor = Color.FromArgb(51, 58, 71);
            btnBrowse.FlatAppearance.MouseDownBackColor = Color.FromArgb(52, 58, 72);
            btnBrowse.FlatAppearance.MouseOverBackColor = Color.FromArgb(40, 45, 56);
            btnBrowse.FlatStyle = FlatStyle.Flat;
            btnBrowse.Font = new Font("Segoe UI", 9F, FontStyle.Bold);
            btnBrowse.ForeColor = Color.FromArgb(232, 233, 235);
            btnBrowse.Location = new Point(464, 44);
            btnBrowse.Name = "btnBrowse";
            btnBrowse.Size = new Size(120, 30);
            btnBrowse.TabIndex = 1;
            btnBrowse.Text = "Browse…";
            btnBrowse.UseVisualStyleBackColor = false;
            btnBrowse.Click += btnBrowse_Click;
            // 
            // btnLaunch
            // 
            btnLaunch.BackColor = Color.FromArgb(229, 72, 77);
            btnLaunch.Cursor = Cursors.Hand;
            btnLaunch.FlatAppearance.BorderColor = Color.FromArgb(242, 85, 90);
            btnLaunch.FlatAppearance.MouseDownBackColor = Color.FromArgb(176, 58, 63);
            btnLaunch.FlatAppearance.MouseOverBackColor = Color.FromArgb(242, 85, 90);
            btnLaunch.FlatStyle = FlatStyle.Flat;
            btnLaunch.Font = new Font("Segoe UI", 9F, FontStyle.Bold);
            btnLaunch.ForeColor = Color.White;
            btnLaunch.Location = new Point(16, 88);
            btnLaunch.Name = "btnLaunch";
            btnLaunch.Size = new Size(280, 48);
            btnLaunch.TabIndex = 2;
            btnLaunch.Text = "▶  LAUNCH GAME";
            btnLaunch.UseVisualStyleBackColor = false;
            btnLaunch.Click += btnLaunch_Click;
            // 
            // btnStart
            // 
            btnStart.BackColor = Color.FromArgb(229, 72, 77);
            btnStart.Cursor = Cursors.Hand;
            btnStart.FlatAppearance.BorderColor = Color.FromArgb(242, 85, 90);
            btnStart.FlatAppearance.MouseDownBackColor = Color.FromArgb(176, 58, 63);
            btnStart.FlatAppearance.MouseOverBackColor = Color.FromArgb(242, 85, 90);
            btnStart.FlatStyle = FlatStyle.Flat;
            btnStart.Font = new Font("Segoe UI", 9F, FontStyle.Bold);
            btnStart.ForeColor = Color.White;
            btnStart.Location = new Point(308, 88);
            btnStart.Name = "btnStart";
            btnStart.Size = new Size(276, 48);
            btnStart.TabIndex = 3;
            btnStart.Text = "START PROXY";
            btnStart.UseVisualStyleBackColor = false;
            btnStart.Click += btnStart_Click;
            // 
            // lblStatus
            // 
            lblStatus.AutoSize = true;
            lblStatus.BackColor = Color.Transparent;
            lblStatus.Font = new Font("Segoe UI", 9F);
            lblStatus.ForeColor = Color.FromArgb(178, 184, 196);
            lblStatus.Location = new Point(16, 150);
            lblStatus.Name = "lblStatus";
            lblStatus.Size = new Size(64, 15);
            lblStatus.TabIndex = 4;
            lblStatus.Tag = "muted";
            lblStatus.Text = "Status: idle";
            // 
            // labelRealHost
            // 
            labelRealHost.AutoSize = true;
            labelRealHost.BackColor = Color.Transparent;
            labelRealHost.Font = new Font("Segoe UI", 9F);
            labelRealHost.ForeColor = Color.FromArgb(232, 233, 235);
            labelRealHost.Location = new Point(16, 196);
            labelRealHost.Name = "labelRealHost";
            labelRealHost.Size = new Size(42, 15);
            labelRealHost.TabIndex = 5;
            labelRealHost.Text = "Server:";
            // 
            // txtRealHost
            // 
            txtRealHost.BackColor = Color.FromArgb(19, 21, 27);
            txtRealHost.BorderStyle = BorderStyle.FixedSingle;
            txtRealHost.Font = new Font("Segoe UI", 9F);
            txtRealHost.ForeColor = Color.FromArgb(232, 233, 235);
            txtRealHost.Location = new Point(70, 192);
            txtRealHost.Name = "txtRealHost";
            txtRealHost.Size = new Size(170, 23);
            txtRealHost.TabIndex = 4;
            txtRealHost.Text = "145.239.149.84";
            // 
            // labelListenPort
            // 
            labelListenPort.AutoSize = true;
            labelListenPort.BackColor = Color.Transparent;
            labelListenPort.Font = new Font("Segoe UI", 9F);
            labelListenPort.ForeColor = Color.FromArgb(232, 233, 235);
            labelListenPort.Location = new Point(258, 196);
            labelListenPort.Name = "labelListenPort";
            labelListenPort.Size = new Size(32, 15);
            labelListenPort.TabIndex = 6;
            labelListenPort.Text = "Port:";
            // 
            // txtListenPort
            // 
            txtListenPort.BackColor = Color.FromArgb(19, 21, 27);
            txtListenPort.BorderStyle = BorderStyle.FixedSingle;
            txtListenPort.Font = new Font("Segoe UI", 9F);
            txtListenPort.ForeColor = Color.FromArgb(232, 233, 235);
            txtListenPort.Location = new Point(300, 192);
            txtListenPort.Name = "txtListenPort";
            txtListenPort.Size = new Size(70, 23);
            txtListenPort.TabIndex = 5;
            txtListenPort.Text = "44411";
            // 
            // bannerPanel
            // 
            bannerPanel.BackColor = Color.FromArgb(24, 27, 34);
            bannerPanel.Controls.Add(lblBannerMu);
            bannerPanel.Controls.Add(lblBannerName);
            bannerPanel.Controls.Add(lblBannerSub);
            bannerPanel.Controls.Add(bannerLine);
            bannerPanel.Controls.Add(lblBannerFoot);
            bannerPanel.Location = new Point(640, 16);
            bannerPanel.Name = "bannerPanel";
            bannerPanel.Size = new Size(608, 357);
            bannerPanel.TabIndex = 1;
            bannerPanel.Tag = "card";
            // 
            // lblBannerMu
            // 
            lblBannerMu.AutoSize = true;
            lblBannerMu.BackColor = Color.Transparent;
            lblBannerMu.Font = new Font("Segoe UI", 92F, FontStyle.Bold);
            lblBannerMu.ForeColor = Color.FromArgb(227, 179, 65);
            lblBannerMu.Location = new Point(167, 110);
            lblBannerMu.Name = "lblBannerMu";
            lblBannerMu.Size = new Size(276, 163);
            lblBannerMu.TabIndex = 0;
            lblBannerMu.Tag = "keep";
            lblBannerMu.Text = "MU";
            // 
            // lblBannerName
            // 
            lblBannerName.AutoSize = true;
            lblBannerName.BackColor = Color.Transparent;
            lblBannerName.Font = new Font("Segoe UI", 24F, FontStyle.Bold);
            lblBannerName.ForeColor = Color.FromArgb(242, 193, 78);
            lblBannerName.Location = new Point(167, 65);
            lblBannerName.Name = "lblBannerName";
            lblBannerName.Size = new Size(279, 45);
            lblBannerName.TabIndex = 1;
            lblBannerName.Tag = "keep";
            lblBannerName.Text = "B L A C K R O C K";
            // 
            // lblBannerSub
            // 
            lblBannerSub.AutoSize = true;
            lblBannerSub.BackColor = Color.Transparent;
            lblBannerSub.Font = new Font("Segoe UI", 8F);
            lblBannerSub.ForeColor = Color.FromArgb(178, 184, 196);
            lblBannerSub.Location = new Point(258, 281);
            lblBannerSub.Name = "lblBannerSub";
            lblBannerSub.Size = new Size(91, 13);
            lblBannerSub.TabIndex = 2;
            lblBannerSub.Tag = "keep";
            lblBannerSub.Text = "M U   O N L I N E";
            // 
            // bannerLine
            // 
            bannerLine.BackColor = Color.FromArgb(138, 109, 43);
            bannerLine.Location = new Point(207, 276);
            bannerLine.Name = "bannerLine";
            bannerLine.Size = new Size(200, 2);
            bannerLine.TabIndex = 3;
            bannerLine.Tag = "keep";
            // 
            // lblBannerFoot
            // 
            lblBannerFoot.AutoSize = true;
            lblBannerFoot.BackColor = Color.Transparent;
            lblBannerFoot.Font = new Font("Segoe UI", 9F);
            lblBannerFoot.ForeColor = Color.FromArgb(178, 184, 196);
            lblBannerFoot.Location = new Point(246, 308);
            lblBannerFoot.Name = "lblBannerFoot";
            lblBannerFoot.Size = new Size(127, 15);
            lblBannerFoot.TabIndex = 4;
            lblBannerFoot.Tag = "keep";
            lblBannerFoot.Text = "DIRECT HOOK · byp.dll";
            // 
            // infoPanel
            // 
            infoPanel.BackColor = Color.FromArgb(24, 27, 34);
            infoPanel.Controls.Add(lblInfoTitle);
            infoPanel.Controls.Add(lblInfo1);
            infoPanel.Controls.Add(lblInfo2);
            infoPanel.Controls.Add(lblInfo3);
            infoPanel.Controls.Add(lblInfo4);
            infoPanel.Location = new Point(640, 399);
            infoPanel.Name = "infoPanel";
            infoPanel.Size = new Size(608, 252);
            infoPanel.TabIndex = 2;
            infoPanel.Tag = "card";
            // 
            // lblInfoTitle
            // 
            lblInfoTitle.AutoSize = true;
            lblInfoTitle.BackColor = Color.Transparent;
            lblInfoTitle.Font = new Font("Segoe UI", 9.5F, FontStyle.Bold);
            lblInfoTitle.ForeColor = Color.FromArgb(227, 179, 65);
            lblInfoTitle.Location = new Point(20, 18);
            lblInfoTitle.Name = "lblInfoTitle";
            lblInfoTitle.Size = new Size(107, 17);
            lblInfoTitle.TabIndex = 0;
            lblInfoTitle.Tag = "keep";
            lblInfoTitle.Text = "HOW IT WORKS";
            // 
            // lblInfo1
            // 
            lblInfo1.AutoSize = true;
            lblInfo1.BackColor = Color.Transparent;
            lblInfo1.Font = new Font("Segoe UI", 9F);
            lblInfo1.ForeColor = Color.FromArgb(232, 233, 235);
            lblInfo1.Location = new Point(20, 56);
            lblInfo1.Name = "lblInfo1";
            lblInfo1.Size = new Size(288, 15);
            lblInfo1.TabIndex = 1;
            lblInfo1.Tag = "keep";
            lblInfo1.Text = "1.  Launch Game abre o main.exe com o byp injetado";
            // 
            // lblInfo2
            // 
            lblInfo2.AutoSize = true;
            lblInfo2.BackColor = Color.Transparent;
            lblInfo2.Font = new Font("Segoe UI", 9F);
            lblInfo2.ForeColor = Color.FromArgb(232, 233, 235);
            lblInfo2.Location = new Point(20, 92);
            lblInfo2.Name = "lblInfo2";
            lblInfo2.Size = new Size(234, 15);
            lblInfo2.TabIndex = 2;
            lblInfo2.Tag = "keep";
            lblInfo2.Text = "2.  Start Proxy sobe o proxy local (127.0.0.1)";
            // 
            // lblInfo3
            // 
            lblInfo3.AutoSize = true;
            lblInfo3.BackColor = Color.Transparent;
            lblInfo3.Font = new Font("Segoe UI", 9F);
            lblInfo3.ForeColor = Color.FromArgb(232, 233, 235);
            lblInfo3.Location = new Point(20, 128);
            lblInfo3.Name = "lblInfo3";
            lblInfo3.Size = new Size(282, 15);
            lblInfo3.TabIndex = 3;
            lblInfo3.Tag = "keep";
            lblInfo3.Text = "3.  O jogo conecta no proxy, que repassa ao servidor";
            // 
            // lblInfo4
            // 
            lblInfo4.AutoSize = true;
            lblInfo4.BackColor = Color.Transparent;
            lblInfo4.Font = new Font("Segoe UI", 9F);
            lblInfo4.ForeColor = Color.FromArgb(178, 184, 196);
            lblInfo4.Location = new Point(20, 172);
            lblInfo4.Name = "lblInfo4";
            lblInfo4.Size = new Size(288, 15);
            lblInfo4.TabIndex = 4;
            lblInfo4.Tag = "keep";
            lblInfo4.Text = "Cada instância usa portas próprias (base 0, 100, 200…)";
            // 
            // tabPage1
            // 
            tabPage1.BackColor = Color.FromArgb(15, 17, 21);
            tabPage1.Controls.Add(groupBox4);
            tabPage1.Controls.Add(groupBoxPotion);
            tabPage1.Controls.Add(groupBox5);
            tabPage1.Controls.Add(groupBoxAccount);
            tabPage1.Font = new Font("Segoe UI", 9F);
            tabPage1.ForeColor = Color.FromArgb(232, 233, 235);
            tabPage1.Location = new Point(4, 40);
            tabPage1.Margin = new Padding(0);
            tabPage1.Name = "tabPage1";
            tabPage1.Size = new Size(1272, 670);
            tabPage1.TabIndex = 1;
            tabPage1.Text = "Custom Stuff";
            // 
            // groupBox4
            // 
            groupBox4.BackColor = Color.FromArgb(24, 27, 34);
            groupBox4.Controls.Add(chkEnableMonsterCapturing);
            groupBox4.Controls.Add(lstMonsters);
            groupBox4.Font = new Font("Segoe UI", 9.5F, FontStyle.Bold);
            groupBox4.ForeColor = Color.FromArgb(227, 179, 65);
            groupBox4.Location = new Point(16, 16);
            groupBox4.Name = "groupBox4";
            groupBox4.Size = new Size(590, 300);
            groupBox4.TabIndex = 0;
            groupBox4.TabStop = false;
            groupBox4.Text = "Monster List";
            // 
            // chkEnableMonsterCapturing
            // 
            chkEnableMonsterCapturing.AutoSize = true;
            chkEnableMonsterCapturing.BackColor = Color.FromArgb(24, 27, 34);
            chkEnableMonsterCapturing.Checked = true;
            chkEnableMonsterCapturing.CheckState = CheckState.Checked;
            chkEnableMonsterCapturing.FlatAppearance.BorderColor = Color.FromArgb(51, 58, 71);
            chkEnableMonsterCapturing.FlatAppearance.CheckedBackColor = Color.FromArgb(227, 179, 65);
            chkEnableMonsterCapturing.FlatStyle = FlatStyle.Flat;
            chkEnableMonsterCapturing.Font = new Font("Segoe UI", 9F);
            chkEnableMonsterCapturing.ForeColor = Color.FromArgb(232, 233, 235);
            chkEnableMonsterCapturing.Location = new Point(16, 26);
            chkEnableMonsterCapturing.Name = "chkEnableMonsterCapturing";
            chkEnableMonsterCapturing.Size = new Size(161, 19);
            chkEnableMonsterCapturing.TabIndex = 0;
            chkEnableMonsterCapturing.Text = "Enable Monster Capturing";
            chkEnableMonsterCapturing.UseVisualStyleBackColor = false;
            // 
            // lstMonsters
            // 
            lstMonsters.BackColor = Color.FromArgb(19, 21, 27);
            lstMonsters.BorderStyle = BorderStyle.FixedSingle;
            lstMonsters.Columns.AddRange(new ColumnHeader[] { clmMonsterID, clmMonsterName, clmMonsterType, clmLevel, clmRange, clmAttackId, clmPos });
            lstMonsters.Font = new Font("Segoe UI", 9F);
            lstMonsters.ForeColor = Color.FromArgb(232, 233, 235);
            lstMonsters.Location = new Point(16, 54);
            lstMonsters.Name = "lstMonsters";
            lstMonsters.OwnerDraw = true;
            lstMonsters.Size = new Size(558, 224);
            lstMonsters.TabIndex = 1;
            lstMonsters.UseCompatibleStateImageBehavior = false;
            lstMonsters.View = View.Details;
            // 
            // clmMonsterID
            //
            clmMonsterID.Text = "ID";
            clmMonsterID.Width = 55;
            //
            // clmMonsterName
            //
            clmMonsterName.Text = "Name";
            clmMonsterName.Width = 150;
            //
            // clmMonsterType
            //
            clmMonsterType.Text = "Type";
            clmMonsterType.Width = 75;
            //
            // clmLevel
            //
            clmLevel.Text = "Lv";
            clmLevel.Width = 40;
            //
            // clmRange
            //
            clmRange.Text = "Range";
            clmRange.Width = 55;
            //
            // clmAttackId
            //
            clmAttackId.Text = "Atk ID";
            clmAttackId.Width = 65;
            //
            // clmPos
            //
            clmPos.Text = "Pos";
            clmPos.Width = 90;
            // 
            // groupBoxPotion
            // 
            groupBoxPotion.BackColor = Color.FromArgb(24, 27, 34);
            groupBoxPotion.Controls.Add(chkAutoPotion);
            groupBoxPotion.Controls.Add(labelPotionHp);
            groupBoxPotion.Controls.Add(txtPotionHp);
            groupBoxPotion.Controls.Add(labelPotionMp);
            groupBoxPotion.Controls.Add(txtPotionMp);
            groupBoxPotion.Controls.Add(labelPotionInterval);
            groupBoxPotion.Controls.Add(txtPotionInterval);
            groupBoxPotion.Controls.Add(lblPotionState);
            groupBoxPotion.Font = new Font("Segoe UI", 9.5F, FontStyle.Bold);
            groupBoxPotion.ForeColor = Color.FromArgb(227, 179, 65);
            groupBoxPotion.Location = new Point(16, 332);
            groupBoxPotion.Name = "groupBoxPotion";
            groupBoxPotion.Size = new Size(590, 240);
            groupBoxPotion.TabIndex = 1;
            groupBoxPotion.TabStop = false;
            groupBoxPotion.Text = "Auto Potion";
            // 
            // chkAutoPotion
            // 
            chkAutoPotion.AutoSize = true;
            chkAutoPotion.BackColor = Color.FromArgb(24, 27, 34);
            chkAutoPotion.Checked = true;
            chkAutoPotion.CheckState = CheckState.Checked;
            chkAutoPotion.FlatAppearance.BorderColor = Color.FromArgb(51, 58, 71);
            chkAutoPotion.FlatAppearance.CheckedBackColor = Color.FromArgb(227, 179, 65);
            chkAutoPotion.FlatStyle = FlatStyle.Flat;
            chkAutoPotion.Font = new Font("Segoe UI", 9F);
            chkAutoPotion.ForeColor = Color.FromArgb(232, 233, 235);
            chkAutoPotion.Location = new Point(16, 28);
            chkAutoPotion.Name = "chkAutoPotion";
            chkAutoPotion.Size = new Size(137, 19);
            chkAutoPotion.TabIndex = 0;
            chkAutoPotion.Text = "Auto potion (HP/MP)";
            chkAutoPotion.UseVisualStyleBackColor = false;
            // 
            // labelPotionHp
            // 
            labelPotionHp.AutoSize = true;
            labelPotionHp.BackColor = Color.Transparent;
            labelPotionHp.Font = new Font("Segoe UI", 9F);
            labelPotionHp.ForeColor = Color.FromArgb(232, 233, 235);
            labelPotionHp.Location = new Point(16, 70);
            labelPotionHp.Name = "labelPotionHp";
            labelPotionHp.Size = new Size(90, 15);
            labelPotionHp.TabIndex = 1;
            labelPotionHp.Text = "HP abaixo de %";
            // 
            // txtPotionHp
            // 
            txtPotionHp.BackColor = Color.FromArgb(19, 21, 27);
            txtPotionHp.BorderStyle = BorderStyle.FixedSingle;
            txtPotionHp.Font = new Font("Segoe UI", 9F);
            txtPotionHp.ForeColor = Color.FromArgb(232, 233, 235);
            txtPotionHp.Location = new Point(150, 66);
            txtPotionHp.Name = "txtPotionHp";
            txtPotionHp.Size = new Size(60, 23);
            txtPotionHp.TabIndex = 1;
            txtPotionHp.Text = "50";
            // 
            // labelPotionMp
            // 
            labelPotionMp.AutoSize = true;
            labelPotionMp.BackColor = Color.Transparent;
            labelPotionMp.Font = new Font("Segoe UI", 9F);
            labelPotionMp.ForeColor = Color.FromArgb(232, 233, 235);
            labelPotionMp.Location = new Point(270, 70);
            labelPotionMp.Name = "labelPotionMp";
            labelPotionMp.Size = new Size(92, 15);
            labelPotionMp.TabIndex = 2;
            labelPotionMp.Text = "MP abaixo de %";
            // 
            // txtPotionMp
            // 
            txtPotionMp.BackColor = Color.FromArgb(19, 21, 27);
            txtPotionMp.BorderStyle = BorderStyle.FixedSingle;
            txtPotionMp.Font = new Font("Segoe UI", 9F);
            txtPotionMp.ForeColor = Color.FromArgb(232, 233, 235);
            txtPotionMp.Location = new Point(404, 66);
            txtPotionMp.Name = "txtPotionMp";
            txtPotionMp.Size = new Size(60, 23);
            txtPotionMp.TabIndex = 2;
            txtPotionMp.Text = "30";
            // 
            // labelPotionInterval
            // 
            labelPotionInterval.AutoSize = true;
            labelPotionInterval.BackColor = Color.Transparent;
            labelPotionInterval.Font = new Font("Segoe UI", 9F);
            labelPotionInterval.ForeColor = Color.FromArgb(232, 233, 235);
            labelPotionInterval.Location = new Point(16, 112);
            labelPotionInterval.Name = "labelPotionInterval";
            labelPotionInterval.Size = new Size(80, 15);
            labelPotionInterval.TabIndex = 3;
            labelPotionInterval.Text = "Intervalo (ms)";
            // 
            // txtPotionInterval
            // 
            txtPotionInterval.BackColor = Color.FromArgb(19, 21, 27);
            txtPotionInterval.BorderStyle = BorderStyle.FixedSingle;
            txtPotionInterval.Font = new Font("Segoe UI", 9F);
            txtPotionInterval.ForeColor = Color.FromArgb(232, 233, 235);
            txtPotionInterval.Location = new Point(150, 108);
            txtPotionInterval.Name = "txtPotionInterval";
            txtPotionInterval.Size = new Size(80, 23);
            txtPotionInterval.TabIndex = 3;
            txtPotionInterval.Text = "1000";
            // 
            // lblPotionState
            // 
            lblPotionState.BackColor = Color.FromArgb(19, 21, 27);
            lblPotionState.BorderStyle = BorderStyle.FixedSingle;
            lblPotionState.Font = new Font("Segoe UI", 9F);
            lblPotionState.ForeColor = Color.FromArgb(232, 233, 235);
            lblPotionState.Location = new Point(16, 150);
            lblPotionState.Name = "lblPotionState";
            lblPotionState.Size = new Size(558, 64);
            lblPotionState.TabIndex = 4;
            lblPotionState.Text = "auto-potion: aguardando o byp.dll...";
            // 
            // groupBox5
            // 
            groupBox5.BackColor = Color.FromArgb(24, 27, 34);
            groupBox5.Controls.Add(chkEnableAttack);
            groupBox5.Controls.Add(chkAttackMonsters);
            groupBox5.Controls.Add(chkAttackPlayerAll);
            groupBox5.Controls.Add(chkAttackPlayerClick);
            groupBox5.Controls.Add(chkAttackOthers);
            groupBox5.Controls.Add(rbNormalAttack);
            groupBox5.Controls.Add(rbSkill);
            groupBox5.Controls.Add(cbSkills);
            groupBox5.Controls.Add(label3);
            groupBox5.Controls.Add(txtRange);
            groupBox5.Controls.Add(label4);
            groupBox5.Controls.Add(txtDelay);
            groupBox5.Controls.Add(label5);
            groupBox5.Controls.Add(txtHitCount);
            groupBox5.Controls.Add(lblAttackHint);
            groupBox5.Font = new Font("Segoe UI", 9.5F, FontStyle.Bold);
            groupBox5.ForeColor = Color.FromArgb(227, 179, 65);
            groupBox5.Location = new Point(622, 16);
            groupBox5.Name = "groupBox5";
            groupBox5.Size = new Size(590, 378);
            groupBox5.TabIndex = 2;
            groupBox5.TabStop = false;
            groupBox5.Text = "Attack Config";
            // 
            // chkEnableAttack
            // 
            chkEnableAttack.AutoSize = true;
            chkEnableAttack.BackColor = Color.FromArgb(24, 27, 34);
            chkEnableAttack.FlatAppearance.BorderColor = Color.FromArgb(51, 58, 71);
            chkEnableAttack.FlatAppearance.CheckedBackColor = Color.FromArgb(227, 179, 65);
            chkEnableAttack.FlatStyle = FlatStyle.Flat;
            chkEnableAttack.Font = new Font("Segoe UI", 9F);
            chkEnableAttack.ForeColor = Color.FromArgb(232, 233, 235);
            chkEnableAttack.Location = new Point(16, 28);
            chkEnableAttack.Name = "chkEnableAttack";
            chkEnableAttack.Size = new Size(95, 19);
            chkEnableAttack.TabIndex = 0;
            chkEnableAttack.Text = "Enable Attack";
            chkEnableAttack.UseVisualStyleBackColor = false;
            // 
            // chkAttackMonsters
            // 
            chkAttackMonsters.AutoSize = true;
            chkAttackMonsters.BackColor = Color.FromArgb(24, 27, 34);
            chkAttackMonsters.FlatAppearance.BorderColor = Color.FromArgb(51, 58, 71);
            chkAttackMonsters.FlatAppearance.CheckedBackColor = Color.FromArgb(227, 179, 65);
            chkAttackMonsters.FlatStyle = FlatStyle.Flat;
            chkAttackMonsters.Font = new Font("Segoe UI", 9F);
            chkAttackMonsters.ForeColor = Color.FromArgb(232, 233, 235);
            chkAttackMonsters.Location = new Point(16, 50);
            chkAttackMonsters.Name = "chkAttackMonsters";
            chkAttackMonsters.Size = new Size(80, 19);
            chkAttackMonsters.TabIndex = 8;
            chkAttackMonsters.Text = "Monstro";
            chkAttackMonsters.UseVisualStyleBackColor = false;
            chkAttackMonsters.Checked = true;
            // 
            // chkAttackPlayerAll
            // 
            chkAttackPlayerAll.AutoSize = true;
            chkAttackPlayerAll.BackColor = Color.FromArgb(24, 27, 34);
            chkAttackPlayerAll.FlatAppearance.BorderColor = Color.FromArgb(51, 58, 71);
            chkAttackPlayerAll.FlatAppearance.CheckedBackColor = Color.FromArgb(227, 179, 65);
            chkAttackPlayerAll.FlatStyle = FlatStyle.Flat;
            chkAttackPlayerAll.Font = new Font("Segoe UI", 9F);
            chkAttackPlayerAll.ForeColor = Color.FromArgb(232, 233, 235);
            chkAttackPlayerAll.Location = new Point(16, 72);
            chkAttackPlayerAll.Name = "chkAttackPlayerAll";
            chkAttackPlayerAll.Size = new Size(160, 19);
            chkAttackPlayerAll.TabIndex = 9;
            chkAttackPlayerAll.Text = "Player - Atacar todos";
            chkAttackPlayerAll.UseVisualStyleBackColor = false;
            // 
            // chkAttackPlayerClick
            // 
            chkAttackPlayerClick.AutoSize = true;
            chkAttackPlayerClick.BackColor = Color.FromArgb(24, 27, 34);
            chkAttackPlayerClick.FlatAppearance.BorderColor = Color.FromArgb(51, 58, 71);
            chkAttackPlayerClick.FlatAppearance.CheckedBackColor = Color.FromArgb(227, 179, 65);
            chkAttackPlayerClick.FlatStyle = FlatStyle.Flat;
            chkAttackPlayerClick.Font = new Font("Segoe UI", 9F);
            chkAttackPlayerClick.ForeColor = Color.FromArgb(232, 233, 235);
            chkAttackPlayerClick.Location = new Point(16, 94);
            chkAttackPlayerClick.Name = "chkAttackPlayerClick";
            chkAttackPlayerClick.Size = new Size(180, 19);
            chkAttackPlayerClick.TabIndex = 10;
            chkAttackPlayerClick.Text = "Player - Atacar no clique";
            chkAttackPlayerClick.UseVisualStyleBackColor = false;
            // 
            // chkAttackOthers
            // 
            chkAttackOthers.AutoSize = true;
            chkAttackOthers.BackColor = Color.FromArgb(24, 27, 34);
            chkAttackOthers.FlatAppearance.BorderColor = Color.FromArgb(51, 58, 71);
            chkAttackOthers.FlatAppearance.CheckedBackColor = Color.FromArgb(227, 179, 65);
            chkAttackOthers.FlatStyle = FlatStyle.Flat;
            chkAttackOthers.Font = new Font("Segoe UI", 9F);
            chkAttackOthers.ForeColor = Color.FromArgb(232, 233, 235);
            chkAttackOthers.Location = new Point(16, 116);
            chkAttackOthers.Name = "chkAttackOthers";
            chkAttackOthers.Size = new Size(80, 19);
            chkAttackOthers.TabIndex = 11;
            chkAttackOthers.Text = "Outros";
            chkAttackOthers.UseVisualStyleBackColor = false;
            // 
            // rbNormalAttack
            // 
            rbNormalAttack.AutoSize = true;
            rbNormalAttack.BackColor = Color.FromArgb(24, 27, 34);
            rbNormalAttack.FlatAppearance.BorderColor = Color.FromArgb(51, 58, 71);
            rbNormalAttack.FlatAppearance.CheckedBackColor = Color.FromArgb(227, 179, 65);
            rbNormalAttack.FlatStyle = FlatStyle.Flat;
            rbNormalAttack.Font = new Font("Segoe UI", 9F);
            rbNormalAttack.ForeColor = Color.FromArgb(232, 233, 235);
            rbNormalAttack.Location = new Point(16, 144);
            rbNormalAttack.Name = "rbNormalAttack";
            rbNormalAttack.Size = new Size(85, 19);
            rbNormalAttack.TabIndex = 1;
            rbNormalAttack.TabStop = true;
            rbNormalAttack.Text = "Normal Atk";
            rbNormalAttack.UseVisualStyleBackColor = false;
            // 
            // rbSkill
            // 
            rbSkill.AutoSize = true;
            rbSkill.BackColor = Color.FromArgb(24, 27, 34);
            rbSkill.FlatAppearance.BorderColor = Color.FromArgb(51, 58, 71);
            rbSkill.FlatAppearance.CheckedBackColor = Color.FromArgb(227, 179, 65);
            rbSkill.FlatStyle = FlatStyle.Flat;
            rbSkill.Font = new Font("Segoe UI", 9F);
            rbSkill.ForeColor = Color.FromArgb(232, 233, 235);
            rbSkill.Location = new Point(107, 144);
            rbSkill.Name = "rbSkill";
            rbSkill.Size = new Size(45, 19);
            rbSkill.TabIndex = 2;
            rbSkill.TabStop = true;
            rbSkill.Text = "Skill";
            rbSkill.UseVisualStyleBackColor = false;
            // 
            // cbSkills
            // 
            cbSkills.BackColor = Color.FromArgb(19, 21, 27);
            cbSkills.FlatStyle = FlatStyle.Flat;
            cbSkills.Font = new Font("Segoe UI", 9F);
            cbSkills.ForeColor = Color.FromArgb(232, 233, 235);
            cbSkills.FormattingEnabled = true;
            cbSkills.Location = new Point(210, 140);
            cbSkills.Name = "cbSkills";
            cbSkills.Size = new Size(193, 23);
            cbSkills.TabIndex = 3;
            // 
            // label3
            // 
            label3.AutoSize = true;
            label3.BackColor = Color.Transparent;
            label3.Font = new Font("Segoe UI", 9F);
            label3.ForeColor = Color.FromArgb(232, 233, 235);
            label3.Location = new Point(16, 194);
            label3.Name = "label3";
            label3.Size = new Size(40, 15);
            label3.TabIndex = 4;
            label3.Text = "Range";
            // 
            // txtRange
            // 
            txtRange.BackColor = Color.FromArgb(19, 21, 27);
            txtRange.BorderStyle = BorderStyle.FixedSingle;
            txtRange.Font = new Font("Segoe UI", 9F);
            txtRange.ForeColor = Color.FromArgb(232, 233, 235);
            txtRange.Location = new Point(80, 190);
            txtRange.Name = "txtRange";
            txtRange.Size = new Size(51, 23);
            txtRange.TabIndex = 4;
            txtRange.Text = "8";
            // 
            // label4
            // 
            label4.AutoSize = true;
            label4.BackColor = Color.Transparent;
            label4.Font = new Font("Segoe UI", 9F);
            label4.ForeColor = Color.FromArgb(232, 233, 235);
            label4.Location = new Point(16, 234);
            label4.Name = "label4";
            label4.Size = new Size(36, 15);
            label4.TabIndex = 5;
            label4.Text = "Delay";
            // 
            // txtDelay
            // 
            txtDelay.BackColor = Color.FromArgb(19, 21, 27);
            txtDelay.BorderStyle = BorderStyle.FixedSingle;
            txtDelay.Font = new Font("Segoe UI", 9F);
            txtDelay.ForeColor = Color.FromArgb(232, 233, 235);
            txtDelay.Location = new Point(80, 230);
            txtDelay.Name = "txtDelay";
            txtDelay.Size = new Size(51, 23);
            txtDelay.TabIndex = 5;
            txtDelay.Text = "300";
            // 
            // label5
            // 
            label5.AutoSize = true;
            label5.BackColor = Color.Transparent;
            label5.Font = new Font("Segoe UI", 9F);
            label5.ForeColor = Color.FromArgb(232, 233, 235);
            label5.Location = new Point(16, 274);
            label5.Name = "label5";
            label5.Size = new Size(28, 15);
            label5.TabIndex = 6;
            label5.Text = "Hits";
            // 
            // txtHitCount
            // 
            txtHitCount.BackColor = Color.FromArgb(19, 21, 27);
            txtHitCount.BorderStyle = BorderStyle.FixedSingle;
            txtHitCount.Font = new Font("Segoe UI", 9F);
            txtHitCount.ForeColor = Color.FromArgb(232, 233, 235);
            txtHitCount.Location = new Point(80, 270);
            txtHitCount.Name = "txtHitCount";
            txtHitCount.Size = new Size(51, 23);
            txtHitCount.TabIndex = 6;
            txtHitCount.Text = "1";
            // 
            // lblAttackHint
            // 
            lblAttackHint.AutoSize = true;
            lblAttackHint.BackColor = Color.Transparent;
            lblAttackHint.Font = new Font("Segoe UI", 9F);
            lblAttackHint.ForeColor = Color.FromArgb(178, 184, 196);
            lblAttackHint.Location = new Point(16, 318);
            lblAttackHint.Name = "lblAttackHint";
            lblAttackHint.Size = new Size(162, 15);
            lblAttackHint.TabIndex = 7;
            lblAttackHint.Tag = "muted";
            lblAttackHint.Text = "F3 abre o skill builder no jogo";
            // 
            // groupBoxAccount
            // 
            groupBoxAccount.BackColor = Color.FromArgb(24, 27, 34);
            groupBoxAccount.Controls.Add(lblAccount);
            groupBoxAccount.Controls.Add(txtAccount);
            groupBoxAccount.Controls.Add(lblPassword);
            groupBoxAccount.Controls.Add(txtPassword);
            groupBoxAccount.Controls.Add(btnAccountApply);
            groupBoxAccount.Controls.Add(lblAccountInfo);
            groupBoxAccount.Font = new Font("Segoe UI", 9.5F, FontStyle.Bold);
            groupBoxAccount.ForeColor = Color.FromArgb(227, 179, 65);
            groupBoxAccount.Location = new Point(622, 410);
            groupBoxAccount.Name = "groupBoxAccount";
            groupBoxAccount.Size = new Size(590, 240);
            groupBoxAccount.TabIndex = 3;
            groupBoxAccount.TabStop = false;
            groupBoxAccount.Text = "Conta do auto-login";
            // 
            // lblAccount
            // 
            lblAccount.AutoSize = true;
            lblAccount.BackColor = Color.Transparent;
            lblAccount.Font = new Font("Segoe UI", 9F);
            lblAccount.ForeColor = Color.FromArgb(232, 233, 235);
            lblAccount.Location = new Point(16, 30);
            lblAccount.Name = "lblAccount";
            lblAccount.Size = new Size(42, 15);
            lblAccount.TabIndex = 0;
            lblAccount.Text = "Conta:";
            // 
            // txtAccount
            // 
            txtAccount.BackColor = Color.FromArgb(19, 21, 27);
            txtAccount.BorderStyle = BorderStyle.FixedSingle;
            txtAccount.Font = new Font("Segoe UI", 9F);
            txtAccount.ForeColor = Color.FromArgb(232, 233, 235);
            txtAccount.Location = new Point(90, 26);
            txtAccount.Name = "txtAccount";
            txtAccount.Size = new Size(240, 23);
            txtAccount.TabIndex = 0;
            // 
            // lblPassword
            // 
            lblPassword.AutoSize = true;
            lblPassword.BackColor = Color.Transparent;
            lblPassword.Font = new Font("Segoe UI", 9F);
            lblPassword.ForeColor = Color.FromArgb(232, 233, 235);
            lblPassword.Location = new Point(16, 70);
            lblPassword.Name = "lblPassword";
            lblPassword.Size = new Size(42, 15);
            lblPassword.TabIndex = 1;
            lblPassword.Text = "Senha:";
            // 
            // txtPassword
            // 
            txtPassword.BackColor = Color.FromArgb(19, 21, 27);
            txtPassword.BorderStyle = BorderStyle.FixedSingle;
            txtPassword.Font = new Font("Segoe UI", 9F);
            txtPassword.ForeColor = Color.FromArgb(232, 233, 235);
            txtPassword.Location = new Point(90, 66);
            txtPassword.Name = "txtPassword";
            txtPassword.Size = new Size(240, 23);
            txtPassword.TabIndex = 1;
            txtPassword.UseSystemPasswordChar = true;
            // 
            // btnAccountApply
            // 
            btnAccountApply.BackColor = Color.FromArgb(31, 35, 45);
            btnAccountApply.Cursor = Cursors.Hand;
            btnAccountApply.FlatAppearance.BorderColor = Color.FromArgb(51, 58, 71);
            btnAccountApply.FlatAppearance.MouseDownBackColor = Color.FromArgb(52, 58, 72);
            btnAccountApply.FlatAppearance.MouseOverBackColor = Color.FromArgb(40, 45, 56);
            btnAccountApply.FlatStyle = FlatStyle.Flat;
            btnAccountApply.Font = new Font("Segoe UI", 9F, FontStyle.Bold);
            btnAccountApply.ForeColor = Color.FromArgb(232, 233, 235);
            btnAccountApply.Location = new Point(346, 64);
            btnAccountApply.Name = "btnAccountApply";
            btnAccountApply.Size = new Size(220, 30);
            btnAccountApply.TabIndex = 2;
            btnAccountApply.Text = "Salvar na sessão";
            btnAccountApply.UseVisualStyleBackColor = false;
            btnAccountApply.Click += btnAccountApply_Click;
            // 
            // lblAccountInfo
            // 
            lblAccountInfo.BackColor = Color.Transparent;
            lblAccountInfo.Font = new Font("Segoe UI", 9F);
            lblAccountInfo.ForeColor = Color.FromArgb(178, 184, 196);
            lblAccountInfo.Location = new Point(16, 112);
            lblAccountInfo.Name = "lblAccountInfo";
            lblAccountInfo.Size = new Size(558, 100);
            lblAccountInfo.TabIndex = 3;
            lblAccountInfo.Tag = "muted";
            lblAccountInfo.Text = "Cada instância tem o seu próprio Settings.ini (pasta da sessão). Duas contas diferentes = o servidor não derruba a segunda.";
            // 
            // tabPackets
            // 
            tabPackets.BackColor = Color.FromArgb(15, 17, 21);
            tabPackets.Controls.Add(gbPacket);
            tabPackets.Controls.Add(lvPackets);
            tabPackets.Font = new Font("Segoe UI", 9F);
            tabPackets.ForeColor = Color.FromArgb(232, 233, 235);
            tabPackets.Location = new Point(4, 40);
            tabPackets.Name = "tabPackets";
            tabPackets.Size = new Size(1272, 670);
            tabPackets.TabIndex = 2;
            tabPackets.Text = "Packets";
            // 
            // gbPacket
            // 
            gbPacket.BackColor = Color.FromArgb(24, 27, 34);
            gbPacket.Controls.Add(groupBox3);
            gbPacket.Controls.Add(groupBox2);
            gbPacket.Dock = DockStyle.Top;
            gbPacket.Font = new Font("Segoe UI", 9.5F, FontStyle.Bold);
            gbPacket.ForeColor = Color.FromArgb(227, 179, 65);
            gbPacket.Location = new Point(0, 0);
            gbPacket.Name = "gbPacket";
            gbPacket.Padding = new Padding(0);
            gbPacket.Size = new Size(1272, 150);
            gbPacket.TabIndex = 0;
            gbPacket.TabStop = false;
            gbPacket.Text = "Packet Management";
            // 
            // groupBox3
            // 
            groupBox3.BackColor = Color.FromArgb(24, 27, 34);
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
            groupBox3.Font = new Font("Segoe UI", 9.5F, FontStyle.Bold);
            groupBox3.ForeColor = Color.FromArgb(227, 179, 65);
            groupBox3.Location = new Point(12, 26);
            groupBox3.Name = "groupBox3";
            groupBox3.Size = new Size(347, 108);
            groupBox3.TabIndex = 0;
            groupBox3.TabStop = false;
            groupBox3.Text = "Protocol Filter";
            // 
            // label2
            // 
            label2.AutoSize = true;
            label2.BackColor = Color.Transparent;
            label2.Font = new Font("Segoe UI", 9F);
            label2.ForeColor = Color.FromArgb(178, 184, 196);
            label2.Location = new Point(138, 18);
            label2.Name = "label2";
            label2.Size = new Size(138, 15);
            label2.TabIndex = 0;
            label2.Tag = "muted";
            label2.Text = "ALL | UNKNOWN | nome";
            // 
            // txtTypeFilter
            // 
            txtTypeFilter.BackColor = Color.FromArgb(19, 21, 27);
            txtTypeFilter.BorderStyle = BorderStyle.FixedSingle;
            txtTypeFilter.Font = new Font("Segoe UI", 9F);
            txtTypeFilter.ForeColor = Color.FromArgb(232, 233, 235);
            txtTypeFilter.Location = new Point(138, 38);
            txtTypeFilter.Name = "txtTypeFilter";
            txtTypeFilter.Size = new Size(200, 23);
            txtTypeFilter.TabIndex = 8;
            // 
            // cbC4
            // 
            cbC4.AutoSize = true;
            cbC4.BackColor = Color.FromArgb(24, 27, 34);
            cbC4.Checked = true;
            cbC4.CheckState = CheckState.Checked;
            cbC4.FlatAppearance.BorderColor = Color.FromArgb(51, 58, 71);
            cbC4.FlatAppearance.CheckedBackColor = Color.FromArgb(227, 179, 65);
            cbC4.FlatStyle = FlatStyle.Flat;
            cbC4.Font = new Font("Segoe UI", 9F);
            cbC4.ForeColor = Color.FromArgb(232, 233, 235);
            cbC4.Location = new Point(8, 23);
            cbC4.Name = "cbC4";
            cbC4.Size = new Size(37, 19);
            cbC4.TabIndex = 3;
            cbC4.Text = "C4";
            cbC4.UseVisualStyleBackColor = false;
            cbC4.CheckedChanged += cbC4_CheckedChanged;
            // 
            // cbC1
            // 
            cbC1.AutoSize = true;
            cbC1.BackColor = Color.FromArgb(24, 27, 34);
            cbC1.Checked = true;
            cbC1.CheckState = CheckState.Checked;
            cbC1.FlatAppearance.BorderColor = Color.FromArgb(51, 58, 71);
            cbC1.FlatAppearance.CheckedBackColor = Color.FromArgb(227, 179, 65);
            cbC1.FlatStyle = FlatStyle.Flat;
            cbC1.Font = new Font("Segoe UI", 9F);
            cbC1.ForeColor = Color.FromArgb(232, 233, 235);
            cbC1.Location = new Point(8, 42);
            cbC1.Name = "cbC1";
            cbC1.Size = new Size(37, 19);
            cbC1.TabIndex = 0;
            cbC1.Text = "C1";
            cbC1.UseVisualStyleBackColor = false;
            // 
            // btnBypassPkt
            // 
            btnBypassPkt.BackColor = Color.FromArgb(229, 72, 77);
            btnBypassPkt.Cursor = Cursors.Hand;
            btnBypassPkt.FlatAppearance.BorderColor = Color.FromArgb(242, 85, 90);
            btnBypassPkt.FlatAppearance.MouseDownBackColor = Color.FromArgb(176, 58, 63);
            btnBypassPkt.FlatAppearance.MouseOverBackColor = Color.FromArgb(242, 85, 90);
            btnBypassPkt.FlatStyle = FlatStyle.Flat;
            btnBypassPkt.Font = new Font("Segoe UI", 9F, FontStyle.Bold);
            btnBypassPkt.ForeColor = Color.White;
            btnBypassPkt.Location = new Point(228, 67);
            btnBypassPkt.Name = "btnBypassPkt";
            btnBypassPkt.Size = new Size(110, 28);
            btnBypassPkt.TabIndex = 7;
            btnBypassPkt.Text = "Bypass Pkt";
            btnBypassPkt.UseVisualStyleBackColor = false;
            btnBypassPkt.Click += btnBypassPkt_Click;
            // 
            // cbC2
            // 
            cbC2.AutoSize = true;
            cbC2.BackColor = Color.FromArgb(24, 27, 34);
            cbC2.Checked = true;
            cbC2.CheckState = CheckState.Checked;
            cbC2.FlatAppearance.BorderColor = Color.FromArgb(51, 58, 71);
            cbC2.FlatAppearance.CheckedBackColor = Color.FromArgb(227, 179, 65);
            cbC2.FlatStyle = FlatStyle.Flat;
            cbC2.Font = new Font("Segoe UI", 9F);
            cbC2.ForeColor = Color.FromArgb(232, 233, 235);
            cbC2.Location = new Point(8, 61);
            cbC2.Name = "cbC2";
            cbC2.Size = new Size(37, 19);
            cbC2.TabIndex = 1;
            cbC2.Text = "C2";
            cbC2.UseVisualStyleBackColor = false;
            // 
            // cbC3
            // 
            cbC3.AutoSize = true;
            cbC3.BackColor = Color.FromArgb(24, 27, 34);
            cbC3.Checked = true;
            cbC3.CheckState = CheckState.Checked;
            cbC3.FlatAppearance.BorderColor = Color.FromArgb(51, 58, 71);
            cbC3.FlatAppearance.CheckedBackColor = Color.FromArgb(227, 179, 65);
            cbC3.FlatStyle = FlatStyle.Flat;
            cbC3.Font = new Font("Segoe UI", 9F);
            cbC3.ForeColor = Color.FromArgb(232, 233, 235);
            cbC3.Location = new Point(8, 80);
            cbC3.Name = "cbC3";
            cbC3.Size = new Size(37, 19);
            cbC3.TabIndex = 2;
            cbC3.Text = "C3";
            cbC3.UseVisualStyleBackColor = false;
            // 
            // btnClearPacket
            // 
            btnClearPacket.BackColor = Color.FromArgb(31, 35, 45);
            btnClearPacket.Cursor = Cursors.Hand;
            btnClearPacket.FlatAppearance.BorderColor = Color.FromArgb(51, 58, 71);
            btnClearPacket.FlatAppearance.MouseDownBackColor = Color.FromArgb(52, 58, 72);
            btnClearPacket.FlatAppearance.MouseOverBackColor = Color.FromArgb(40, 45, 56);
            btnClearPacket.FlatStyle = FlatStyle.Flat;
            btnClearPacket.Font = new Font("Segoe UI", 9F, FontStyle.Bold);
            btnClearPacket.ForeColor = Color.FromArgb(232, 233, 235);
            btnClearPacket.Location = new Point(138, 67);
            btnClearPacket.Name = "btnClearPacket";
            btnClearPacket.Size = new Size(84, 28);
            btnClearPacket.TabIndex = 6;
            btnClearPacket.Text = "Clear";
            btnClearPacket.UseVisualStyleBackColor = false;
            btnClearPacket.Click += btnClearPacket_Click;
            // 
            // cbOther
            // 
            cbOther.AutoSize = true;
            cbOther.BackColor = Color.FromArgb(24, 27, 34);
            cbOther.Checked = true;
            cbOther.CheckState = CheckState.Checked;
            cbOther.FlatAppearance.BorderColor = Color.FromArgb(51, 58, 71);
            cbOther.FlatAppearance.CheckedBackColor = Color.FromArgb(227, 179, 65);
            cbOther.FlatStyle = FlatStyle.Flat;
            cbOther.Font = new Font("Segoe UI", 9F);
            cbOther.ForeColor = Color.FromArgb(232, 233, 235);
            cbOther.Location = new Point(51, 23);
            cbOther.Name = "cbOther";
            cbOther.Size = new Size(59, 19);
            cbOther.TabIndex = 4;
            cbOther.Text = "Outros";
            cbOther.UseVisualStyleBackColor = false;
            // 
            // cbAutoScroll
            // 
            cbAutoScroll.AutoSize = true;
            cbAutoScroll.BackColor = Color.FromArgb(24, 27, 34);
            cbAutoScroll.Checked = true;
            cbAutoScroll.CheckState = CheckState.Checked;
            cbAutoScroll.FlatAppearance.BorderColor = Color.FromArgb(51, 58, 71);
            cbAutoScroll.FlatAppearance.CheckedBackColor = Color.FromArgb(227, 179, 65);
            cbAutoScroll.FlatStyle = FlatStyle.Flat;
            cbAutoScroll.Font = new Font("Segoe UI", 9F);
            cbAutoScroll.ForeColor = Color.FromArgb(232, 233, 235);
            cbAutoScroll.Location = new Point(51, 42);
            cbAutoScroll.Name = "cbAutoScroll";
            cbAutoScroll.Size = new Size(81, 19);
            cbAutoScroll.TabIndex = 5;
            cbAutoScroll.Text = "Auto Scroll";
            cbAutoScroll.UseVisualStyleBackColor = false;
            // 
            // groupBox2
            // 
            groupBox2.BackColor = Color.FromArgb(24, 27, 34);
            groupBox2.Controls.Add(txtPacket);
            groupBox2.Controls.Add(rbServer);
            groupBox2.Controls.Add(rbClient);
            groupBox2.Controls.Add(btnSendPacketDecoded);
            groupBox2.Font = new Font("Segoe UI", 9.5F, FontStyle.Bold);
            groupBox2.ForeColor = Color.FromArgb(227, 179, 65);
            groupBox2.Location = new Point(624, 26);
            groupBox2.Name = "groupBox2";
            groupBox2.Size = new Size(579, 108);
            groupBox2.TabIndex = 1;
            groupBox2.TabStop = false;
            groupBox2.Text = "Packet Sender";
            // 
            // txtPacket
            // 
            txtPacket.BackColor = Color.FromArgb(19, 21, 27);
            txtPacket.BorderStyle = BorderStyle.FixedSingle;
            txtPacket.Font = new Font("Segoe UI", 9F);
            txtPacket.ForeColor = Color.FromArgb(232, 233, 235);
            txtPacket.Location = new Point(16, 62);
            txtPacket.Name = "txtPacket";
            txtPacket.Size = new Size(528, 23);
            txtPacket.TabIndex = 3;
            // 
            // rbServer
            // 
            rbServer.AutoSize = true;
            rbServer.BackColor = Color.FromArgb(24, 27, 34);
            rbServer.FlatAppearance.BorderColor = Color.FromArgb(51, 58, 71);
            rbServer.FlatAppearance.CheckedBackColor = Color.FromArgb(227, 179, 65);
            rbServer.FlatStyle = FlatStyle.Flat;
            rbServer.Font = new Font("Segoe UI", 9F);
            rbServer.ForeColor = Color.FromArgb(232, 233, 235);
            rbServer.Location = new Point(90, 24);
            rbServer.Name = "rbServer";
            rbServer.Size = new Size(56, 19);
            rbServer.TabIndex = 1;
            rbServer.TabStop = true;
            rbServer.Text = "Server";
            rbServer.UseVisualStyleBackColor = false;
            rbServer.CheckedChanged += rbServer_CheckedChanged;
            // 
            // rbClient
            // 
            rbClient.AutoSize = true;
            rbClient.BackColor = Color.FromArgb(24, 27, 34);
            rbClient.FlatAppearance.BorderColor = Color.FromArgb(51, 58, 71);
            rbClient.FlatAppearance.CheckedBackColor = Color.FromArgb(227, 179, 65);
            rbClient.FlatStyle = FlatStyle.Flat;
            rbClient.Font = new Font("Segoe UI", 9F);
            rbClient.ForeColor = Color.FromArgb(232, 233, 235);
            rbClient.Location = new Point(16, 24);
            rbClient.Name = "rbClient";
            rbClient.Size = new Size(55, 19);
            rbClient.TabIndex = 0;
            rbClient.TabStop = true;
            rbClient.Text = "Client";
            rbClient.UseVisualStyleBackColor = false;
            rbClient.CheckedChanged += rbClient_CheckedChanged;
            // 
            // btnSendPacketDecoded
            // 
            btnSendPacketDecoded.BackColor = Color.FromArgb(229, 72, 77);
            btnSendPacketDecoded.Cursor = Cursors.Hand;
            btnSendPacketDecoded.FlatAppearance.BorderColor = Color.FromArgb(242, 85, 90);
            btnSendPacketDecoded.FlatAppearance.MouseDownBackColor = Color.FromArgb(176, 58, 63);
            btnSendPacketDecoded.FlatAppearance.MouseOverBackColor = Color.FromArgb(242, 85, 90);
            btnSendPacketDecoded.FlatStyle = FlatStyle.Flat;
            btnSendPacketDecoded.Font = new Font("Segoe UI", 9F, FontStyle.Bold);
            btnSendPacketDecoded.ForeColor = Color.White;
            btnSendPacketDecoded.Location = new Point(380, 18);
            btnSendPacketDecoded.Name = "btnSendPacketDecoded";
            btnSendPacketDecoded.Size = new Size(160, 32);
            btnSendPacketDecoded.TabIndex = 2;
            btnSendPacketDecoded.Text = "SEND PACKET";
            btnSendPacketDecoded.UseVisualStyleBackColor = false;
            btnSendPacketDecoded.Click += btnSendPacketDecoded_Click;
            // 
            // lvPackets
            // 
            lvPackets.Alignment = ListViewAlignment.SnapToGrid;
            lvPackets.BackColor = Color.FromArgb(19, 21, 27);
            lvPackets.BorderStyle = BorderStyle.FixedSingle;
            lvPackets.Columns.AddRange(new ColumnHeader[] { clmOrigin, clmData, clmLenght, clmType, clmPacket, clmPlain });
            lvPackets.Dock = DockStyle.Fill;
            lvPackets.Font = new Font("Segoe UI", 9F);
            lvPackets.ForeColor = Color.FromArgb(232, 233, 235);
            lvPackets.FullRowSelect = true;
            lvPackets.Location = new Point(0, 0);
            lvPackets.Name = "lvPackets";
            lvPackets.OwnerDraw = true;
            lvPackets.Size = new Size(1272, 670);
            lvPackets.TabIndex = 1;
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
            tabLogs.BackColor = Color.FromArgb(15, 17, 21);
            tabLogs.Controls.Add(txtLog);
            tabLogs.Controls.Add(logsTop);
            tabLogs.Font = new Font("Segoe UI", 9F);
            tabLogs.ForeColor = Color.FromArgb(232, 233, 235);
            tabLogs.Location = new Point(4, 40);
            tabLogs.Name = "tabLogs";
            tabLogs.Size = new Size(1272, 670);
            tabLogs.TabIndex = 3;
            tabLogs.Text = "Logs";
            // 
            // txtLog
            // 
            txtLog.BackColor = Color.FromArgb(19, 21, 27);
            txtLog.BorderStyle = BorderStyle.FixedSingle;
            txtLog.DetectUrls = false;
            txtLog.Dock = DockStyle.Fill;
            txtLog.Font = new Font("Consolas", 9.5F);
            txtLog.ForeColor = Color.FromArgb(232, 233, 235);
            txtLog.Location = new Point(0, 36);
            txtLog.Name = "txtLog";
            txtLog.ReadOnly = true;
            txtLog.Size = new Size(1272, 634);
            txtLog.TabIndex = 1;
            txtLog.Text = "";
            txtLog.WordWrap = false;
            // 
            // logsTop
            // 
            logsTop.BackColor = Color.FromArgb(24, 27, 34);
            logsTop.Controls.Add(btnClear);
            logsTop.Dock = DockStyle.Top;
            logsTop.Location = new Point(0, 0);
            logsTop.Name = "logsTop";
            logsTop.Size = new Size(1272, 36);
            logsTop.TabIndex = 2;
            logsTop.Tag = "header";
            // 
            // btnClear
            // 
            btnClear.BackColor = Color.FromArgb(31, 35, 45);
            btnClear.Cursor = Cursors.Hand;
            btnClear.Dock = DockStyle.Right;
            btnClear.FlatAppearance.BorderColor = Color.FromArgb(51, 58, 71);
            btnClear.FlatAppearance.MouseDownBackColor = Color.FromArgb(52, 58, 72);
            btnClear.FlatAppearance.MouseOverBackColor = Color.FromArgb(40, 45, 56);
            btnClear.FlatStyle = FlatStyle.Flat;
            btnClear.Font = new Font("Segoe UI", 9F, FontStyle.Bold);
            btnClear.ForeColor = Color.FromArgb(232, 233, 235);
            btnClear.Location = new Point(1182, 0);
            btnClear.Name = "btnClear";
            btnClear.Size = new Size(90, 36);
            btnClear.TabIndex = 0;
            btnClear.Text = "Clear";
            btnClear.UseVisualStyleBackColor = false;
            btnClear.Click += btnClear_Click;
            // 
            // statusStrip1
            // 
            statusStrip1.BackColor = Color.FromArgb(24, 27, 34);
            statusStrip1.Font = new Font("Segoe UI", 9F);
            statusStrip1.ForeColor = Color.FromArgb(178, 184, 196);
            statusStrip1.Items.AddRange(new ToolStripItem[] { toolTipPlayerCoord, toolTipPlayerInfo });
            statusStrip1.Location = new Point(0, 778);
            statusStrip1.Name = "statusStrip1";
            statusStrip1.Size = new Size(1280, 22);
            statusStrip1.SizingGrip = false;
            statusStrip1.TabIndex = 0;
            statusStrip1.Text = "statusStrip1";
            // 
            // toolTipPlayerCoord
            // 
            toolTipPlayerCoord.Font = new Font("Segoe UI", 9F);
            toolTipPlayerCoord.ForeColor = Color.FromArgb(178, 184, 196);
            toolTipPlayerCoord.Name = "toolTipPlayerCoord";
            toolTipPlayerCoord.Size = new Size(35, 17);
            toolTipPlayerCoord.Text = "[X, Y]";
            // 
            // toolTipPlayerInfo
            // 
            toolTipPlayerInfo.Font = new Font("Segoe UI", 9F);
            toolTipPlayerInfo.ForeColor = Color.FromArgb(178, 184, 196);
            toolTipPlayerInfo.Name = "toolTipPlayerInfo";
            toolTipPlayerInfo.Size = new Size(1230, 17);
            toolTipPlayerInfo.Spring = true;
            toolTipPlayerInfo.Text = "Idx:0x0000 | Pos:(0,0) | Range:0 | Entities:0 | Monsters:0";
            // 
            // MainForm
            // 
            AutoScaleDimensions = new SizeF(7F, 15F);
            AutoScaleMode = AutoScaleMode.Font;
            BackColor = Color.FromArgb(15, 17, 21);
            ClientSize = new Size(1280, 800);
            Controls.Add(tabs);
            Controls.Add(statusStrip1);
            Controls.Add(headerPanel);
            Font = new Font("Segoe UI", 9F);
            ForeColor = Color.FromArgb(232, 233, 235);
            MinimumSize = new Size(1280, 800);
            Name = "MainForm";
            StartPosition = FormStartPosition.CenterScreen;
            Text = "MuProxy2 — BlackrockMU";
            headerPanel.ResumeLayout(false);
            headerPanel.PerformLayout();
            tabs.ResumeLayout(false);
            tabGeneral.ResumeLayout(false);
            groupBox1.ResumeLayout(false);
            groupBox1.PerformLayout();
            bannerPanel.ResumeLayout(false);
            bannerPanel.PerformLayout();
            infoPanel.ResumeLayout(false);
            infoPanel.PerformLayout();
            tabPage1.ResumeLayout(false);
            groupBox4.ResumeLayout(false);
            groupBox4.PerformLayout();
            groupBoxPotion.ResumeLayout(false);
            groupBoxPotion.PerformLayout();
            groupBox5.ResumeLayout(false);
            groupBox5.PerformLayout();
            groupBoxAccount.ResumeLayout(false);
            groupBoxAccount.PerformLayout();
            tabPackets.ResumeLayout(false);
            gbPacket.ResumeLayout(false);
            groupBox3.ResumeLayout(false);
            groupBox3.PerformLayout();
            groupBox2.ResumeLayout(false);
            groupBox2.PerformLayout();
            tabLogs.ResumeLayout(false);
            logsTop.ResumeLayout(false);
            statusStrip1.ResumeLayout(false);
            statusStrip1.PerformLayout();
            ResumeLayout(false);
            PerformLayout();
        }

        #endregion

        private Panel headerPanel;
        private Label lblSword;
        private Label lblTitle;
        private Label lblSubtitle;
        private Label lblSeason;
        private Label lblVersion;
        private Panel headerLine;
        private TabControl tabs;
        private TabPage tabGeneral;
        private TabPage tabPage1;
        private TabPage tabPackets;
        private TabPage tabLogs;
        private GroupBox groupBox1;
        private Label lblExePath;
        private TextBox txtExePath;
        private Button btnBrowse;
        private Button btnLaunch;
        private Button btnStart;
        private Label lblStatus;
        private Label labelRealHost;
        private TextBox txtRealHost;
        private Label labelListenPort;
        private TextBox txtListenPort;
        private Panel bannerPanel;
        private Label lblBannerMu;
        private Label lblBannerName;
        private Label lblBannerSub;
        private Panel bannerLine;
        private Label lblBannerFoot;
        private Panel infoPanel;
        private Label lblInfoTitle;
        private Label lblInfo1;
        private Label lblInfo2;
        private Label lblInfo3;
        private Label lblInfo4;
        private GroupBox groupBox4;
        private CheckBox chkEnableMonsterCapturing;
        private ListView lstMonsters;
        private ColumnHeader clmMonsterID;
        private ColumnHeader clmMonsterName;
        private ColumnHeader clmMonsterType;
        private ColumnHeader clmLevel;
        private ColumnHeader clmRange;
        private ColumnHeader clmAttackId;
        private ColumnHeader clmPos;
        private GroupBox groupBoxPotion;
        private CheckBox chkAutoPotion;
        private Label labelPotionHp;
        private TextBox txtPotionHp;
        private Label labelPotionMp;
        private TextBox txtPotionMp;
        private Label labelPotionInterval;
        private TextBox txtPotionInterval;
        private Label lblPotionState;
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
        private CheckBox chkAttackMonsters;
        private CheckBox chkAttackPlayerAll;
        private CheckBox chkAttackPlayerClick;
        private CheckBox chkAttackOthers;
        private Label lblAttackHint;
        private GroupBox groupBoxAccount;
        private Label lblAccount;
        private TextBox txtAccount;
        private Label lblPassword;
        private TextBox txtPassword;
        private Button btnAccountApply;
        private Label lblAccountInfo;
        private StatusStrip statusStrip1;
        private ToolStripStatusLabel toolTipPlayerCoord;
        private ToolStripStatusLabel toolTipPlayerInfo;
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
        private ColumnHeader clmPlain;
        private GroupBox groupBox2;
        private GroupBox groupBox3;
        private Label label2;
        private TextBox txtTypeFilter;
        private CheckBox cbC4;
        private CheckBox cbC1;
        private Button btnBypassPkt;
        private CheckBox cbC2;
        private CheckBox cbC3;
        private CheckBox cbOther;
        private CheckBox cbAutoScroll;
        private Button btnSendPacketDecoded;
        private Panel logsTop;
        private Button btnClear;
        private RichTextBox txtLog;
    }
}
