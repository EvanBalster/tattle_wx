//
//  app.cpp
//  tattle
//
//  Created by Evan Balster on 11/12/16.
//

#include <fstream> // Debug

#include "tattle.h"

#include <wx/app.h>
#include <wx/cmdline.h>

//File reading / encoding
#include <wx/file.h>


namespace tattle
{
	class TattleApp;
}

using namespace tattle;

static PersistentData persist_;
static Report   report_;
static UIConfig uiConfig_ = UIConfig(report_.config);

PersistentData &tattle::persist = persist_;
const Report   &tattle::report   = report_;
const UIConfig &tattle::uiConfig = uiConfig_;

UIConfig::UIConfig(Json & _config) :
	config(_config)
{
}

unsigned UIConfig::marginSm() const    {return JsonFetch(config, "/style/margin_small", 5u);}
unsigned UIConfig::marginMd() const    {return JsonFetch(config, "/style/margin_medium", 8u);}
unsigned UIConfig::marginLg() const    {return JsonFetch(config, "/style/margin_large", 10u);}

wxArtID UIConfig::GetIconID(wxString tattleName) const
{
	if (tattleName == "information") return wxART_INFORMATION;
	if (tattleName == "info")        return wxART_INFORMATION;
	if (tattleName == "warning")     return wxART_WARNING;
	if (tattleName == "error")       return wxART_ERROR;
	if (tattleName == "question")    return wxART_QUESTION;
	if (tattleName == "help")        return wxART_HELP;
	if (tattleName == "tip")         return wxART_TIP;
	return "";
}

static TattleApp *tattleApp = NULL;


namespace tattle
{
	extern void Tattle_OnInitCmdLine (wxCmdLineParser& parser);
	extern bool Tattle_ExecCmdLine   (Json& config, wxCmdLineParser& parser);
	extern bool Tattle_ExecCmdLineArg(Json &config, const wxCmdLineArg &arg);
}


class tattle::TattleApp : public wxApp
{
public:
	friend void tattle::Tattle_Halt();


	static bool ParsePair(const wxString &str, wxString &first, wxString &second)
	{
		wxString::size_type p = str.find_first_of('=');
		if (p == wxString::npos || p == 0) return false;

		first = str.substr(0, p);
		second = str.substr(p + 1);
		return true;
	}

	enum CMD_LINE_ERR
	{
		CMD_ERR_NONE = 0,
		CMD_ERR_UNKNOWN,
		CMD_ERR_BAD_PAIR,
		CMD_ERR_CONTENT_REDECLARED,
		CMD_ERR_CONTENT_MISSING,
		CMD_ERR_CONTENT_NOT_APPLICABLE,
		CMD_ERR_FAILED_TO_OPEN_CONFIG,
		CMD_ERR_FAILED_TO_OPEN_FILE,
		CMD_ERR_BAD_URL,
		CMD_ERR_BAD_SIZE,
	};

	virtual void OnInitCmdLine(wxCmdLineParser& parser) wxOVERRIDE
	{
		Tattle_OnInitCmdLine(parser);
	}
	
	virtual bool OnCmdLineParsed(wxCmdLineParser& parser) wxOVERRIDE
	{
		return Tattle_ExecCmdLine(report_.config, parser);
	}

	enum RUN_STAGE
	{
		RS_START = 0,
		RS_QUERY,
		RS_PROMPT,
		RS_POST,
		RS_DONE,
	};

	void ToggleIdleHandler(bool active)
	{
		if (idleHandler == active) return;
		idleHandler = active;
		if (active) Connect(wxID_ANY, wxEVT_IDLE, wxIdleEventHandler(TattleApp::OnIdle));
		else        Disconnect(wxEVT_IDLE, wxIdleEventHandler(TattleApp::OnIdle));
	}

	/*
		This is a crude coroutine describing the workflow of a Tattle execution.
			It is called once on initialization, and again whenever a dialog completes.
			It executes until another dialog is created or the program completes.

	*/
	void Proceed()
	{
		// Make sure this doesn't carry over...
		pendingWindow = NULL;

		// Proceed
		switch (stage)
		{
		case RS_START:
			stage = RS_QUERY;
			PerformQuery();
			if (pendingWindow) break;
			[[fallthrough]];

		case RS_QUERY:
			if (stage <= RS_PROMPT) // Allow skipping via Halt()
			{
				stage = RS_PROMPT;
				PerformPrompt();
			}
			if (pendingWindow) break;
			[[fallthrough]];

		case RS_PROMPT:
			if (stage <= RS_PROMPT) // Allow skipping via Halt()
			{
				stage = RS_POST;
				PerformPost();
			}
			if (pendingWindow) break;
			[[fallthrough]];

		case RS_POST:
			stage = RS_DONE;
			[[fallthrough]];

		case RS_DONE:
			if (prompt)
			{
				prompt->Destroy();
				prompt = NULL;
			}
			stage = RS_DONE;
			std::cout << "Done now..." << std::endl;
			printTopLevelWindows();
			break;
		}

		// Show any pending window
		if (pendingWindow)
		{
			anyWindows = true;
			pendingWindow->Show();
			pendingWindow->Raise();
			//pendingWindow = NULL;
		}

		// Register the idle handler to start the next task
		if (stage != RS_DONE) ToggleIdleHandler(true);
	}

	void ShowPrompt()
	{
		if (stage < RS_PROMPT)
		{
			stage = RS_QUERY;
			Proceed();
		}
		else if (prompt)
		{
			// Go back to the prompt step
			stage = RS_PROMPT;
			if (!prompt->IsEnabled()) prompt->Enable();
			if (!prompt->IsShown()) prompt->Show();
			prompt->Raise();
		}
		else
		{
			wxASSERT("Tattle workflow error: can't resume prompt");
		}
	}

	void InsertDialog(wxWindow *dialog)
	{
		pendingWindow = dialog;

		ToggleIdleHandler(true);
	}

	void DisposeDialog(wxWindow *dialog)
	{
		disposeWindow = dialog;

		ToggleIdleHandler(true);
	}

	void Halt()
	{
		stage = RS_DONE;
		Proceed();
	}

	void OnIdle(wxIdleEvent &event)
	{
		if (disposeWindow)
		{
			disposeWindow->Destroy();
			disposeWindow = NULL;
			return;
		}

		if (pendingWindow)
		{
			pendingWindow->Show();
			pendingWindow->Raise();
			pendingWindow = NULL;
		}

		// Unregister this handler
		ToggleIdleHandler(false);

		if (stage == RS_DONE)
		{
			std::cout << "Done and idle..." << std::endl;
			if (printTopLevelWindows())
				ToggleIdleHandler(true);
		}

		/*switch (stage)
		{
		case RS_START:
		case RS_QUERY:
			//Proceed();
			break;

		case RS_DONE:
			break;
		}*/
	}
	
	static bool printTopLevelWindows()
	{
		// Hmm
		wxWindowList::compatibility_iterator node = wxTopLevelWindows.GetFirst();
		if (node)
		{
			std::cout << "Top level windows: ";
			while (node)
			{
				wxTopLevelWindow* win = (wxTopLevelWindow*) node->GetData();
				
				std::cout
					// << "@" << win
					// << ":" << wxString(win->GetClassInfo()->GetClassName())
					<< "\"" << win->GetTitle() << "\"";
				
				node = node->GetNext();
				
				if (node) std::cout << ", ";
			}
			std::cout << std::endl;
		}
		
		return node != NULL;
	}

	// this one is called on application startup and is a good place for the app
	// initialization (doing it here and not in the ctor allows to have an error
	// return: if OnInit() returns false, the application terminates)
	virtual bool OnInit() wxOVERRIDE;

	virtual int OnExit() wxOVERRIDE;
	
	virtual int OnRun() wxOVERRIDE;

	void PerformQuery();
	void PerformPrompt();
	void PerformPost();

private:
	RUN_STAGE stage;

	wxWindow* pendingWindow, * disposeWindow;
	Prompt* prompt;

	bool idleHandler;
	bool anyWindows;
};

bool TattleApp::OnInit()
{
	anyWindows = false;

	//cout << "Reading command line..." << endl;

	// call the base class initialization method, currently it only parses a
	// few common command-line options but it could be do more in the future
	if (!wxApp::OnInit())
		return false;

	tattleApp = this;
	stage = RS_START;
	pendingWindow = NULL;
	disposeWindow = NULL;
	prompt = NULL;

	bool badCmdLine = false;

	// DEBUG: dump the config to CWD
	if (report.config.contains("$DEBUG_DUMP"))
	{
		wxFile file(std::string(report.config["$DEBUG_DUMP"]), wxFile::OpenMode::write);
		if (!file.IsOpened()) return false;

		file.Write(report.config.dump(1, '\t', false, nlohmann::detail::error_handler_t::replace));

		file.Close();
	}

	if (!report.url_post().isSet() && !report.url_query().isSet())
	{
		cout << "At least one URL must be set with the --url-* options." << endl;
		badCmdLine = true;
	}

	if (badCmdLine)
	{
		cout << "  Execute tattle --help for more information." << endl;
		return false;
	}

	report_.compile();

	if (report.contents().size() == 0)
	{
		cout << "The report is empty.  Supply at least one piece of content (string, input or file)." << endl;
		badCmdLine = true;
	}

	// Debug
	cout << "Successful init." << endl
		<< "  URL (post): " << report.url_post().full()
		<< "  Contents:" << endl;
	for (Report::Contents::const_iterator i = report.contents().begin(); i != report.contents().end(); ++i)
		cout << "    - " << i->name << "= `" << i->value() << "' Type#" << i->type << endl;

	Proceed();

	return true;
}

int TattleApp::OnExit()
{
	//cout << "Exiting..." << endl;
	return wxApp::OnExit();
}

void TattleApp::PerformQuery()
{
	/*
		Pre-query step, if applicable
	*/
	if (report.url_query().isSet())
	{
		Report::Reply reply = report.httpQuery(*this);

		if (reply.valid() && !uiConfig.silentQuery())
		{
			// Queue up the prompt window
			pendingWindow = Prompt::DisplayReply(reply);
		}
		else
		{
			// Flag for a connection warning.
			report_.connectionWarning = true;
		}
	}
	else if (!uiConfig.silentQuery())
	{
		// Probe the server to test the connection
		if (report.url_post().isSet() && !report.httpTest(*this, report.url_post()))
			report_.connectionWarning = true;
	}
}
void TattleApp::PerformPrompt()
{
	// Skip if no post or running silently
	if (report.url_post().isSet() && !uiConfig.silentPost())
	{
		if (persist.shouldShow(report_.identity()))
		{
			// Set up the prompt window for display
			prompt = new Prompt(NULL, -1, report_);
			pendingWindow = prompt;
		}
		else
		{
			//TODO prevent submission
			Halt();
		}
	}
}
void TattleApp::PerformPost()
{
	// Could be query-only...
	if (!report.url_post().isSet()) return;
	
	Report::Reply reply = report.httpPost(prompt ? (wxEvtHandler&) *prompt : *this);

	if (reply.valid())
	{
		if (!reply.icon.length()) reply.icon = wxART_INFORMATION;
	}
	else
	{
		if (!reply.icon.length()) reply.icon = wxART_ERROR;

		// Flag for a connection warning.
		report_.connectionWarning = true;
	}

	// Queue up the info dialog
	if (!uiConfig.silentPost())
		pendingWindow = Prompt::DisplayReply(reply, prompt);
}

/*int TattleApp::OnIdle()
{
	//
}*/

int TattleApp::OnRun()
{
	if (!anyWindows)
	{
		ExitMainLoop();
		return 0;
	}
	
	return wxApp::OnRun();
}

void tattle::Tattle_Proceed()
{
	tattleApp->Proceed();
}
void tattle::Tattle_ShowPrompt()
{
	tattleApp->ShowPrompt();
}
void tattle::Tattle_InsertDialog(wxWindow *dialog)
{
	tattleApp->InsertDialog(dialog);
}
void tattle::Tattle_DisposeDialog(wxWindow *dialog)
{
	tattleApp->DisposeDialog(dialog);
}
void tattle::Tattle_Halt()
{
	tattleApp->stage = TattleApp::RS_DONE;
	tattleApp->Halt();
}

// the application icon (under Windows it is in resources and even
// though we could still include the XPM here it would be unused)
/*#ifndef wxHAS_IMAGES_IN_RESOURCES
#include "../sample.xpm"
#endif*/

// 
wxIMPLEMENT_APP(tattle::TattleApp);
