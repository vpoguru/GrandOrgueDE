/*
 * OurOrgan - free pipe organ simulator based on MyOrgan
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 */

#include "MyOrgan.h"
#include "ToolbarImages.h"
#include "KeyConvert.h"

#ifdef __WIN32__
#include <windows.h>
#include <shlobj.h>
#endif

#if 0
#define DEBUGHEAP
#include <malloc.h>
void _heapwalk_()
{
    FILE *debug = fopen("c:\\log.txt", "w");
    _HEAPINFO hinfo;
    hinfo._pentry = NULL;
    while (_heapwalk(&hinfo) == _HEAPOK)
        fprintf(debug, "%p %8d %s\n", hinfo._pentry, hinfo._size, hinfo._useflag == _USEDENTRY ? "USED" : "FREE" );
    fclose(debug);
}
#endif

// Code to add the Splash Screen JPEG from MyOrgan.rc as a memory input stream
wxMemoryInputStream *GetResourceInputStream(wxString resource_name, wxString resource_type){
        HRSRC hrsrc=FindResource(wxGetInstance(), resource_name, resource_type);
        if(hrsrc==NULL) return NULL;

        HGLOBAL hglobal=LoadResource(wxGetInstance(), hrsrc);
        if(hglobal==NULL) return NULL;

        void *data=LockResource(hglobal);
        if(data==NULL) return NULL;

        DWORD datalen=SizeofResource(wxGetInstance(), hrsrc);
        if(datalen<1) return NULL;

        return new wxMemoryInputStream(data, datalen);
}

bool HasResource(wxString resource_name, wxString resource_type){
        HRSRC hrsrc=FindResource(wxGetInstance(), resource_name, resource_type);
        if(hrsrc==NULL) return false;

        HGLOBAL hglobal=LoadResource(wxGetInstance(), hrsrc);
        if(hglobal==NULL) return false;

        return true;
}
// End

IMPLEMENT_APP(MyApp)

extern const unsigned char* Images_Wood[];
extern int c_Images_Wood[];
extern MySound* g_sound;
extern MyOrganFile* organfile;

void AsyncLoadFile(wxString what)
{
    MyApp* app = &::wxGetApp();
    if (!app->frame || !app->m_docManager)
        return;

    wxFileName fn(what);
    fn.Normalize();
    wxCommandEvent event(wxEVT_LOADFILE, 0);
    event.SetString(fn.GetFullPath());
    app->frame->AddPendingEvent(event);
}

class stConnection : public wxConnection
{
public:
    stConnection() { }
    ~stConnection() { }
    bool OnExecute(const wxString& topic, wxChar* data, int size, wxIPCFormat format)
    {
        MyApp* app = &::wxGetApp();

        app->frame->Raise();
        if (data[0])
            ::AsyncLoadFile(data);

        return true;
    }
};

class stServer : public wxServer
{
public:
    wxConnectionBase* OnAcceptConnection(const wxString& topic)
    {
        MyApp* app = &::wxGetApp();
        if (!app->frame || !app->m_docManager)
            return false;

        if (topic == "open")
            return new stConnection();
        return 0;
    }
};

class stClient : public wxClient
{
public:
    stClient() { }
    wxConnectionBase* OnMakeConnection() { return new stConnection; }
};

class MyDocManager : public wxDocManager
{
public:
    void OnUpdateFileSave(wxUpdateUIEvent& event);
    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(MyDocManager, wxDocManager)
    EVT_UPDATE_UI(wxID_SAVE, MyDocManager::OnUpdateFileSave)
END_EVENT_TABLE();

void MyDocManager::OnUpdateFileSave(wxUpdateUIEvent& event)
{
    wxDocument *doc = GetCurrentDocument();
    event.Enable( doc );
}

MyApp::MyApp()
{
}

bool MyApp::OnInit()
{
    int lang = m_locale.GetSystemLanguage();

    switch(lang)
    {
    case wxLANGUAGE_GERMAN:
    case wxLANGUAGE_GERMAN_AUSTRIAN:
    case wxLANGUAGE_GERMAN_BELGIUM:
    case wxLANGUAGE_GERMAN_LIECHTENSTEIN:
    case wxLANGUAGE_GERMAN_LUXEMBOURG:
    case wxLANGUAGE_GERMAN_SWISS:
        m_locale.Init(wxLANGUAGE_GERMAN);
        m_locale.AddCatalog(wxT("myorgan"));
        break;
    default:
        break;
    }

    int IsPentiumMMX;

    asm("\
	    xorl %%eax,%%eax       \n\
	    pushal                 \n\
	    pushfl                 \n\
	    popl %%eax             \n\
	    movl %%eax,%%ecx       \n\
	    xorl $0x200000,%%eax   \n\
	    pushl %%eax            \n\
	    popfl                  \n\
	    pushfl                 \n\
	    popl %%eax             \n\
	    cmpl %%ecx,%%eax       \n\
	    jz CpuDone             \n\
	    movl $1,%%eax          \n\
	    cpuid                  \n\
	    test $0x800000,%%edx   \n\
    CpuDone:                   \n\
	    popal                  \n\
	    setnz %%al             \n\
    " : "=a" (IsPentiumMMX) );

    if (!IsPentiumMMX)
    {
        ::wxLogError(wxT("Your processor does not support the MMX(R) instruction set."));
        return false;
    }

	soundSystem = 0;
	m_docManager = 0;

	single_instance = new wxSingleInstanceChecker(APP_NAME);
	if (single_instance->IsAnotherRunning())
	{
	    wxLogNull logNull;
	    stClient* client = new stClient;
	    wxConnectionBase* connection = client->MakeConnection(wxT("localhost"), APP_NAME, wxT("open"));
	    if (connection)
	    {
	        connection->Execute(argc > 1 ? argv[1] : "");
	        connection->Disconnect();
	        delete connection;
	    }
	    delete client;
		return false;
	}
	else
	{
	    m_server = new stServer;
	    if (!m_server->Create(APP_NAME))
            ::wxLogError(wxT("Failed to create IPC service."));
	}

	SetAppName(APP_NAME);
	SetClassName(APP_NAME);
	SetVendorName(wxT("Our Organ"));
	pConfig = wxConfigBase::Get();
	pConfig->SetRecordDefaults();


	wxIdleEvent::SetMode(wxIDLE_PROCESS_SPECIFIED);
    wxFileSystem::AddHandler(new wxZipFSHandler);
	wxImage::AddHandler(new wxJPEGHandler);
	wxImage::AddHandler(new wxGIFHandler);
	srand(::wxGetUTCTime());

	m_help = new wxHtmlHelpController(wxHF_CONTENTS | wxHF_INDEX | wxHF_SEARCH | wxHF_ICONS_BOOK | wxHF_FLAT_TOOLBAR);
        m_help->AddBook(wxFileName(wxT("MyOrgan.htb")));

#ifdef __WIN32__
	SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED);
	char docs[MAX_PATH];
	SHGetFolderPath(NULL, CSIDL_PERSONAL | CSIDL_FLAG_CREATE, NULL, 0, docs);
	m_path = docs;
	m_path += "\\";
#endif
#ifdef linux
    m_path="$HOME"; //TODO??
    //wxFont::SetDefaultEncoding(wxFONTENCODING_CP1250);
#endif
	m_docManager = new MyDocManager;
	new wxDocTemplate(m_docManager, _("Sample set definition files"), "*.organ", wxEmptyString, "organ", "Organ Doc", "Organ View", CLASSINFO(OrganDocument), CLASSINFO(OrganView));
	m_docManager->SetMaxDocsOpen(1);

	soundSystem = new MySound;
	frame = new MyFrame(m_docManager, (wxFrame*)NULL, wxID_ANY, APP_NAME, wxDefaultPosition, wxDefaultSize, wxMINIMIZE_BOX | wxSYSTEM_MENU | wxCAPTION | wxCLOSE_BOX | wxCLIP_CHILDREN | wxFULL_REPAINT_ON_RESIZE);
	frame->DoSplash();
	bool open_sound = soundSystem->OpenSound();
	::wxSleep(2);
	frame->Show(true);

    if (!open_sound)
	{
	    SettingsDialog dialog(frame);
        SetTopWindow(&dialog);
	    dialog.ShowModal();
	}
    SetTopWindow(frame);

    if (argc > 1 && argv[1][0])
    {
        ::AsyncLoadFile(argv[1]);
        argv[1][0] = 0;
        argc = 1;
    }
#ifdef __VFD__
    MyLCD_Open();
#endif

    g_sound->UpdateOrganMIDI(); //retrieve MIDI settings for loading organs

	return true;
}

int MyApp::OnExit()
{
#ifdef __VFD__
    MyLCD_Close();
#endif
	if (soundSystem)
		delete soundSystem;
	if (m_docManager)
	{
		m_docManager->FileHistorySave(*pConfig);
		delete m_docManager;
	}
	delete m_help;
    delete m_server;
    delete single_instance;

    return wxApp::OnExit();
}

IMPLEMENT_CLASS(MyFrame, wxDocParentFrame)
BEGIN_EVENT_TABLE(MyFrame, wxDocParentFrame)
	EVT_COMMAND(0, wxEVT_METERS, MyFrame::OnMeters)
	EVT_COMMAND(0, wxEVT_LOADFILE, MyFrame::OnLoadFile)
    EVT_MENU_OPEN(MyFrame::OnMenuOpen)
	EVT_MENU(ID_FILE_OPEN, MyFrame::OnOpen)
	EVT_MENU_RANGE(wxID_FILE1, wxID_FILE9, MyFrame::OnOpen)
	EVT_MENU(ID_FILE_RELOAD, MyFrame::OnReload)
	EVT_MENU(ID_FILE_REVERT, MyFrame::OnRevert)
	EVT_MENU(ID_FILE_PROPERTIES, MyFrame::OnProperties)
	EVT_MENU(ID_FILE_LOAD, MyFrame::OnLoad)
	EVT_MENU(ID_FILE_SAVE, MyFrame::OnSave)
	EVT_MENU(ID_FILE_CACHE, MyFrame::OnCache)
	EVT_MENU(ID_AUDIO_PANIC, MyFrame::OnAudioPanic)
	EVT_MENU(ID_AUDIO_RECORD, MyFrame::OnAudioRecord)
	EVT_MENU(ID_AUDIO_MEMSET, MyFrame::OnAudioMemset)
	EVT_MENU(ID_AUDIO_SETTINGS, MyFrame::OnAudioSettings)
	EVT_MENU(wxID_HELP, MyFrame::OnHelp)
	EVT_MENU(wxID_ABOUT, MyFrame::OnHelpAbout)
	EVT_KEY_DOWN(MyFrame::OnKeyCommand)
	EVT_KEY_UP(MyFrame::OnKeyCommand)

	EVT_UPDATE_UI(wxID_SAVE, MyFrame::OnUpdateLoaded)
	EVT_UPDATE_UI_RANGE(ID_FILE_RELOAD, ID_AUDIO_MEMSET, MyFrame::OnUpdateLoaded)
END_EVENT_TABLE()

void MyFrame::AddTool(wxMenu* menu, int id, const wxString& item, const wxString& helpString, unsigned char* toolbarImage, int size, wxItemKind kind)
{
	menu->Append(id, item, wxEmptyString, kind);
	if (toolbarImage)
	{
        wxMemoryInputStream mem(toolbarImage, size);
        wxImage img(mem, wxBITMAP_TYPE_GIF);
        GetToolBar()->AddTool(id, item, wxBitmap(img), helpString, kind);
	}
}

MyFrame::MyFrame(wxDocManager *manager, wxFrame *frame, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, const long type)
    : wxDocParentFrame(manager, frame, id, title, pos, size, type)
{
	m_gaugedc = 0;

	SetIcon(wxIcon("#101"));

	wxMenu *file_menu = new wxMenu;

	wxMenu *recent_menu = new wxMenu;
	m_docManager->FileHistoryUseMenu(recent_menu);
	m_docManager->FileHistoryLoad(*wxConfigBase::Get());

	wxToolBar* tb = CreateToolBar(wxNO_BORDER | wxTB_HORIZONTAL | wxTB_FLAT);
	tb->SetToolBitmapSize(wxSize(16, 20));

	AddTool(file_menu, ID_FILE_OPEN, _("&Open...\tCtrl+O"), _("Open"), GIF_Open, GIF_OPEN_LEN);
	file_menu->Append(wxID_ANY, _("Open &Recent"), recent_menu);
	file_menu->AppendSeparator();
	AddTool(file_menu, ID_FILE_RELOAD, _("Re&load"), _("Reload"), GIF_Reload, GIF_RELOAD_LEN);
	AddTool(file_menu, ID_FILE_REVERT, _("Reset to &Defaults"));
	file_menu->AppendSeparator();
	AddTool(file_menu, ID_FILE_LOAD, _("&Import Settings..."));
	AddTool(file_menu, ID_FILE_SAVE, _("&Export Settings..."));
	AddTool(file_menu, ID_FILE_CACHE, _("&Update Cache..."));
	file_menu->AppendSeparator();
	AddTool(file_menu, wxID_SAVE, _("&Save\tCtrl+S"), _("Save"), GIF_Save, GIF_SAVE_LEN);
	AddTool(file_menu, wxID_CLOSE, _("&Close"));
	AddTool(file_menu, ID_FILE_PROPERTIES, _("&Properties..."), _("Properties"), GIF_Property, GIF_PROPERTY_LEN);
	file_menu->AppendSeparator();
	AddTool(file_menu, wxID_EXIT, _("E&xit"));
	tb->AddSeparator();

	wxMenu *audio_menu = new wxMenu;
	AddTool(audio_menu, ID_AUDIO_RECORD, _("&Record...\tCtrl+R"), _("Record"), GIF_Record, GIF_RECORD_LEN, wxITEM_CHECK);
	AddTool(audio_menu, ID_AUDIO_MEMSET, _("&Memory Set\tShift"), _("Memory Set"), GIF_Set, GIF_SET_LEN, wxITEM_CHECK);
	audio_menu->AppendSeparator();
	AddTool(audio_menu, ID_AUDIO_PANIC, _("&Panic\tEscape"), _("Panic"), GIF_Panic, GIF_PANIC_LEN);
	AddTool(audio_menu, ID_AUDIO_SETTINGS, _("&Settings..."), _("Audio Settings"), GIF_Settings, GIF_SETTINGS_LEN);
	tb->AddSeparator();

	wxMenu *help_menu = new wxMenu;
	AddTool(help_menu, wxID_HELP, _("&Help\tF1"), _("Help"), GIF_Help, GIF_HELP_LEN);
	AddTool(help_menu, wxID_ABOUT, _("&About"));
	tb->AddSeparator();

	{
        wxMemoryInputStream mem(GIF_Gauge, GIF_GAUGE_LEN);
        wxImage img(mem, wxBITMAP_TYPE_GIF);
        m_gauge = wxBitmap(img);
	}
	m_gaugedc = new wxMemoryDC();
	m_gaugedc->SelectObject(m_gauge);
	m_meters[0] = new MyMeter(tb, ID_METER_AUDIO_SPIN, 3);
	m_meters[1] = new MyMeter(tb, ID_METER_POLY_SPIN,  2);
	m_meters[2] = new MyMeter(tb, ID_METER_FRAME_SPIN, 1);
	m_meters[3] = new MyMeter(tb, ID_METER_TRANSPOSE_SPIN, 0);
	for (int i = 0; i < 4; i++)
	{
		tb->AddControl(m_meters[i]);
		if (i < 3)
			tb->AddSeparator();
	}

	wxMenuBar *menu_bar = new wxMenuBar;
	menu_bar->Append(file_menu, _("&File"));
	menu_bar->Append(audio_menu, _("&Audio"));
	menu_bar->Append(help_menu, _("&Help"));
	SetMenuBar(menu_bar);
	tb->Realize();

    	SetClientSize(880, 495);	// default minimal size
	Center(wxBOTH);

    m_pedalBrush.SetColour(0xA0, 0x80, 0x40);
}

MyFrame::~MyFrame()
{
    if (m_gaugedc)
    {
        delete m_gaugedc;
        m_gaugedc = 0;
    }
}

void MyFrame::OnMeters(wxCommandEvent& event)
{
	int n = event.GetInt();
	m_meters[0]->SetValue(0, n);
	m_meters[0]->SetValue(1, n >> 8);
	m_meters[1]->SetValue(0, n >> 16);
}

void MyFrame::OnUpdateLoaded(wxUpdateUIEvent& event)
{
	if (g_sound)
	{
		if (event.GetId() == ID_AUDIO_RECORD)
			event.Check(g_sound->f_output && !g_sound->b_stoprecording);
		else if (event.GetId() == ID_AUDIO_MEMSET)
			event.Check(g_sound->b_memset);
	}
	event.Enable(organfile && m_docManager->GetCurrentDocument() && (event.GetId() == ID_FILE_REVERT ? organfile->b_customized : true));
}

void MyFrame::OnLoadFile(wxCommandEvent& event)
{
    if (!IsEnabled())
        return;
    bool prev = false;
    if (organfile)
    {
        prev = organfile->m_opening;
        organfile->m_opening = true;
    }
    m_docManager->CreateDocument(event.GetString(), wxDOC_SILENT);
    if (organfile)  // hopefully so...
        organfile->m_opening = prev;
}

void MyFrame::OnOpen(wxCommandEvent& event)
{
	if (organfile)
		organfile->m_opening = true;
	if (event.GetId() == ID_FILE_OPEN)
	{
		wxFileName fn = wxFileName::GetCwd();
		fn.AppendDir("organs");
		::wxGetApp().m_docManager->SetLastDirectory(wxConfig::Get()->Read("organPath", fn.GetPath()));
		ProcessCommand(wxID_OPEN);
		if (organfile)
		{
			wxConfig::Get()->Write("organPath", ::wxGetApp().m_docManager->GetLastDirectory());
			organfile->m_opening = false;
		}
	}
	else
		event.Skip();
}

void MyFrame::OnLoad(wxCommandEvent& event)
{
	OrganDocument* doc = (OrganDocument*)m_docManager->GetCurrentDocument();
	if (!doc || !organfile)
        return;

    wxFileDialog dlg(::wxGetApp().frame, _("Import Settings"), wxConfig::Get()->Read("cmbPath", ::wxGetApp().m_path + "My Organs"), wxEmptyString, _("Settings files (*.cmb)|*.cmb"), wxOPEN | wxFILE_MUST_EXIST);
    if (dlg.ShowModal() == wxID_OK)
    {
        wxConfig::Get()->Write("cmbPath", dlg.GetDirectory());
        wxString file = organfile->m_filename;
        organfile->m_opening = true;
        doc->DoOpenDocument(file, dlg.GetPath());
        if (organfile)
            organfile->m_opening = false;
    }
}

void MyFrame::OnSave(wxCommandEvent& event)
{
	OrganDocument* doc = (OrganDocument*)m_docManager->GetCurrentDocument();
	if (!doc || !organfile)
        return;

    wxFileDialog dlg(::wxGetApp().frame, _("Export Settings"), wxConfig::Get()->Read("cmbPath", ::wxGetApp().m_path + "My Organs"), wxEmptyString, _("Settings files (*.cmb)|*.cmb"), wxSAVE | wxOVERWRITE_PROMPT);
    if (dlg.ShowModal() == wxID_OK)
    {
        wxConfig::Get()->Write("cmbPath", dlg.GetDirectory());
        doc->DoSaveDocument(dlg.GetPath());
        doc->Modify(false);
    }
}

wxString formatSize(wxLongLong& size)
{
    double n = (double)size.ToLong();
    char sizes[][3] = {"KB", "MB", "GB", "TB"};
    int i;

    for (i = 0; i < 3; i++)
    {
        n /= 1024.0;
        if (n < 1024.0)
            break;
    }
    return wxString::Format("%.2f %s", n, sizes[i]);
}

void MyFrame::OnCache(wxCommandEvent& event)
{
	OrganDocument* doc = (OrganDocument*)m_docManager->GetCurrentDocument();
	if (!doc || !organfile)
        return;

    int i;
    std::vector<int> todo;
    wxLongLong freespace, required = 0;

    {
        wxBusyCursor busy;

        for (i = 0; i < (int)organfile->pipe_filenames.size(); i++)
        {
            if (organfile->pipe_filenames[i].IsEmpty())
                continue;
            wxString temp = organfile->pipe_filenames[i].BeforeLast('-');
            if (!::wxFileExists(organfile->pipe_filenames[i]) || (::wxFileExists(temp) && ::wxFileModificationTime(temp) > ::wxFileModificationTime(organfile->pipe_filenames[i])))
                todo.push_back(i);
        }
        if (todo.empty())
        {
            ::wxLogWarning(_("The sample cache is already up to date."));
            return;
        }

        ::wxGetDiskSpace(wxFileName(organfile->pipe_filenames[todo[0]]).GetPath(), 0, &freespace);
        for (i = 0; i < (int)todo.size(); i++)
            required += organfile->pipe_filesizes[todo[i]];
    }

    int ffile, written;
    if (::wxMessageBox(_(
        "Updating the sample cache decreases\n"
        "load time at the cost of disk space.\n"
        "\n"
        "Required: " ) + formatSize(required)  + _("\n"
        "Available: ") + formatSize(freespace) + _("\n"
        "\n"
        "Continue with this operation?"), APP_NAME, wxYES_NO | wxICON_QUESTION) == wxYES)
    {
        wxProgressDialog dlg(_("Updating sample cache"), wxEmptyString, todo.size(), 0, wxPD_AUTO_HIDE | wxPD_CAN_ABORT | wxPD_APP_MODAL | wxPD_ELAPSED_TIME | wxPD_ESTIMATED_TIME | wxPD_REMAINING_TIME);
        for (i = 0; i < (int)todo.size(); i++)
        {
            char* fn = (char*)organfile->pipe_filenames[todo[i]].c_str();
            if (!dlg.Update(i, fn))
                break;

            #ifdef linux
            ffile = open(fn, O_WRONLY | O_CREAT);
            #endif
            #ifdef __WIN32__
            ffile = _open(fn, _O_BINARY | _O_WRONLY | _O_SEQUENTIAL | _O_CREAT, _S_IREAD | _S_IWRITE);
            #endif
            if (ffile == -1)
            {
                ::wxLogWarning("Could not write to '%s'", fn);
                break;
            }

            MyPipe* pipe = organfile->pipe[todo[i]];
            int size = organfile->pipe_filesizes[todo[i]];
            pipe->_this = pipe;
            pipe->_adler32 = adler32(0, (Bytef*)&pipe->_this, size - offsetof(MyPipe, _this));
            pipe->_fourcc = *(unsigned *)"MyOc";

            #ifdef linux
            written = write(ffile, pipe, size);
            close(ffile);
            #endif
            #ifdef __WIN32__
             written = _write(ffile, pipe, size);
            _close(ffile);
            #endif

            if (written != organfile->pipe_filesizes[todo[i]])
            {
                ::wxRemoveFile(fn);
                ::wxLogWarning("Could not write to '%s'", fn);
                break;
            }
        }
    }
}

void MyFrame::OnReload(wxCommandEvent& event)
{
	ProcessCommand(wxID_FILE1);
}

void MyFrame::OnRevert(wxCommandEvent& event)
{
    if (organfile && m_docManager->GetCurrentDocument() && ::wxMessageBox(_("Any customizations you have saved to this\norgan definition file will be lost!\n\nReset to defaults and reload?"), APP_NAME, wxYES_NO | wxICON_EXCLAMATION, this) == wxYES)
    {
        {
            wxLog::EnableLogging(false);
            wxFileConfig cfg(wxEmptyString, wxEmptyString, organfile->m_filename, wxEmptyString, wxCONFIG_USE_LOCAL_FILE | wxCONFIG_USE_NO_ESCAPE_CHARACTERS);
            wxLog::EnableLogging(true);
            m_docManager->GetCurrentDocument()->Modify(false);
            organfile->Revert(cfg);
        }
        ProcessCommand(wxID_FILE1);
    }
}

void MyFrame::OnProperties(wxCommandEvent& event)
{
	MyProperties dlg(this);
	dlg.ShowModal();
}

void MyFrame::OnAudioPanic(wxCommandEvent& WXUNUSED(event))
{
	if (!g_sound)
		return;
	g_sound->ResetSound();
}

void MyFrame::OnAudioRecord(wxCommandEvent& WXUNUSED(event))
{
	if (!g_sound)
		return;

	if (g_sound->f_output)
		g_sound->b_stoprecording = true;
	else
		g_sound->OpenWAV();
}

void MyFrame::OnAudioMemset(wxCommandEvent& WXUNUSED(event))
{
	if (!::wxGetApp().m_docManager->GetCurrentDocument() || !g_sound)
		return;
	g_sound->b_memset ^= true;
}

void MyFrame::OnAudioSettings(wxCommandEvent& WXUNUSED(event))
{
::wxLogDebug("settingsdialog..");
    SettingsDialog dialog(this);
    ::wxLogDebug("success");
    dialog.ShowModal();
    g_sound->UpdateOrganMIDI();
}

void MyFrame::OnHelp(wxCommandEvent& event)
{
#ifdef DEBUGHEAP
    _heapwalk_();
    return;
#endif
    ::wxGetApp().m_help->Display("User Interface");
}

void MyFrame::OnHelpRegister(wxCommandEvent& event)
{
	MyRegister dlg(this);
	if (dlg.ShowModal() == wxID_OK)
	{
	    wxString str1 = dlg.m_key->GetValue(), str2;
	    for (int i = 0; i < (int)str1.length(); i++)
            if ((str1[i] >= '2' && str1[i] <= '9') || (str1[i] >= 'A' && str1[i] <= 'Z'))
                str2 += str1[i];
        ::wxConfig::Get()->Write("ProductKey", str2);
	    ProcessCommand(wxID_ABOUT);
	}
}

void MyFrame::OnHelpAbout(wxCommandEvent& event)
{
	DoSplash(false);
}

void MyFrame::DoSplash(bool timeout)
{
	/*wxMemoryInputStream mem((const char*)Images_Wood[27], c_Images_Wood[27]);
	wxImage img(mem, wxBITMAP_TYPE_JPEG);
	wxSplashScreenModal* splash = new wxSplashScreenModal(wxBitmap(img), timeout ? wxSPLASH_CENTRE_ON_SCREEN | wxSPLASH_TIMEOUT : wxSPLASH_CENTRE_ON_SCREEN | wxSPLASH_NO_TIMEOUT, 3000, this, wxID_ANY);
	if (!timeout)
		splash->ShowModal();*/
	// *******************************************************************************
	// New Splash Screen using code from wxWidgets Forum
    wxMemoryInputStream* stream = GetResourceInputStream(wxT("SplashImage"), wxT("JPG"));
    if(stream != NULL){
                wxImage image(*stream);
                wxBitmap bitmap(image);

                if (bitmap.Ok()){

                        wxSplashScreen *splash = new wxSplashScreen(bitmap, wxSPLASH_CENTRE_ON_SCREEN|wxSPLASH_TIMEOUT,
                                5000, this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                wxBORDER_NONE|wxSTAY_ON_TOP);
                        wxYield();

                }
        }
}

void MyFrame::OnKeyCommand(wxKeyEvent& event)
{
	if (g_sound && g_sound->b_memset ^ event.ShiftDown())
	{
		ProcessCommand(ID_AUDIO_MEMSET);
		UpdateWindowUI();
	}

	int i, j, k = event.GetKeyCode();
    if (event.GetEventType() == wxEVT_KEY_DOWN && !event.AltDown())
    {
        MyMeter* meter = ::wxGetApp().frame->m_meters[2];
        OrganDocument* doc = (OrganDocument*)m_docManager->GetCurrentDocument();
        switch(k)
        {
        case WXK_ESCAPE:
            ProcessCommand(ID_AUDIO_PANIC);
            break;
        case WXK_LEFT:
            meter->SetValue(meter->GetValue() - 1);
            break;
        case WXK_DOWN:
            if (organfile)
                organfile->framegeneral[meter->GetValue() - 1].Push();
            break;
        case WXK_RIGHT:
            meter->SetValue(meter->GetValue() + 1);
            break;
        default:
            if (organfile && doc && doc->b_loaded && (k = WXKtoVK(k)))
            {
                for (i = organfile->FirstManual; i <= organfile->NumberOfManuals; i++)
                {
                    for (j = 0; j < organfile->manual[i].NumberOfStops; j++)
                        if (k == organfile->manual[i].stop[j].ShortcutKey)
                            organfile->manual[i].stop[j].Push();
                    for (j = 0; j < organfile->manual[i].NumberOfCouplers; j++)
                        if (k == organfile->manual[i].coupler[j].ShortcutKey)
                            organfile->manual[i].coupler[j].Push();
                    for (j = 0; j < organfile->manual[i].NumberOfDivisionals; j++)
                        if (k == organfile->manual[i].divisional[j].ShortcutKey)
                            organfile->manual[i].divisional[j].Push();
                }
                for (j = 0; j < organfile->NumberOfTremulants; j++)
                    if (k == organfile->tremulant[j].ShortcutKey)
                        organfile->tremulant[j].Push();
                for (j = 0; j < organfile->NumberOfDivisionalCouplers; j++)
                    if (k == organfile->divisionalcoupler[j].ShortcutKey)
                        organfile->divisionalcoupler[j].Push();
                for (j = 0; j < organfile->NumberOfGenerals; j++)
                    if (k == organfile->general[j].ShortcutKey)
                        organfile->general[j].Push();
                for (j = 0; j < organfile->NumberOfReversiblePistons; j++)
                    if (k == organfile->piston[j].ShortcutKey)
                        organfile->piston[j].Push();
            }
            event.Skip();
        }
    }
    else
		event.Skip();
}

void MyFrame::OnMenuOpen(wxMenuEvent& event)
{
    DoMenuUpdates(event.GetMenu());
    event.Skip();
}

//////// wxSplashScreenModal

#define wxSPLASH_TIMER_ID 9999

BEGIN_EVENT_TABLE(wxSplashScreenModal, wxDialog)
    EVT_TIMER(wxSPLASH_TIMER_ID, wxSplashScreenModal::OnNotify)
    EVT_CLOSE(wxSplashScreenModal::OnCloseWindow)
END_EVENT_TABLE()

wxSplashScreenModal::wxSplashScreenModal(const wxBitmap& bitmap, long splashStyle, int milliseconds, wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style):
    wxDialog(parent, id, wxEmptyString, wxPoint(0,0), wxSize(100, 100), style)
{
    // At least for GTK+ 2.0, this hint is not available.
/*JB#if defined(__WXGTK20__)
#if GTK_CHECK_VERSION(2,2,0)
    gtk_window_set_type_hint(GTK_WINDOW(m_widget),
                             GDK_WINDOW_TYPE_HINT_SPLASHSCREEN);
#endif
#endif*/

    m_window = NULL;
    m_splashStyle = splashStyle;
    m_milliseconds = milliseconds;

    m_window = new wxSplashScreenWindow(bitmap, this, wxID_ANY, pos, size, wxNO_BORDER);

    SetClientSize(bitmap.GetWidth(), bitmap.GetHeight());

    if (m_splashStyle & wxSPLASH_CENTRE_ON_PARENT)
        CentreOnParent();
    else if (m_splashStyle & wxSPLASH_CENTRE_ON_SCREEN)
        CentreOnScreen();

    if (m_splashStyle & wxSPLASH_TIMEOUT)
    {
        m_timer.SetOwner(this, wxSPLASH_TIMER_ID);
        m_timer.Start(milliseconds, true);
    }

    Show(true);
    m_window->SetFocus();
#if defined( __WXMSW__ ) || defined(__WXMAC__)
    Update(); // Without this, you see a blank screen for an instant
#else
    wxYieldIfNeeded(); // Should eliminate this
#endif
}

wxSplashScreenModal::~wxSplashScreenModal()
{
    m_timer.Stop();
}

void wxSplashScreenModal::OnNotify(wxTimerEvent& WXUNUSED(event))
{
    Close(true);
}

void wxSplashScreenModal::OnCloseWindow(wxCloseEvent& WXUNUSED(event))
{
    m_timer.Stop();
    this->Destroy();
}
