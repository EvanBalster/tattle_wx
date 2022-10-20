//
//  view_report.cpp
//  tattle
//
//  Created by Evan Balster on 11/25/16.
//

#include "tattle.h"

//Dialog stuff
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/statline.h>
#include <wx/sizer.h>

#if wxMAJOR_VERSION >= 3
	#include <wx/wrapsizer.h>
#endif

using namespace tattle;


wxBEGIN_EVENT_TABLE(ViewReport, wxDialog)
	EVT_BUTTON(ViewReport::Ev_Done,     ViewReport::OnDone)
	EVT_BUTTON(ViewReport::Ev_OpenFile, ViewReport::OnOpenFile)
	EVT_BUTTON(ViewReport::Ev_OpenDir,  ViewReport::OnOpenDir)
	EVT_CLOSE (ViewReport::OnClose)
wxEND_EVENT_TABLE()


static int ViewReportCount = 0;

bool ViewReport::Exists()
{
	return ViewReportCount > 0;
}

ViewReport::~ViewReport()
{
	--ViewReportCount;
}

static void ContentDump(wxString &dump, const Report::Content *content)
{
	// Special handling for fields with newlines
	if (content->value().length() > 35 || content->value().find_first_of("\r\n") != wxString::npos)
	{
		wxString valueIndent = content->value();
		valueIndent.Replace(wxString("\n"), wxString("\n| "));
		dump += content->name + wxT(":\n| ") + valueIndent;
	}
	else
	{
		wxString label = content->name+wxT(": ");
		if (label.length() < 20) label.Append(' ', size_t(20-label.length()));
		dump += label + content->value();
	}
}


ViewReport::ViewReport(wxWindow * parent, wxWindowID id)
	: wxDialog(parent, id, wxT("Report Contents"),
		wxDefaultPosition, wxDefaultSize,
		wxDEFAULT_DIALOG_STYLE | uiConfig.style()),
	fontTechnical(wxFontInfo().Family(wxFONTFAMILY_TELETYPE))
{
	SetIcon(wxArtProvider::GetIcon(wxART_INFORMATION));

	const unsigned MARGIN = uiConfig.marginSm();

	++ViewReportCount;
	
	wxBoxSizer *sizerTop = new wxBoxSizer(wxVERTICAL);
	
	sizerTop->AddSpacer(MARGIN);
	
	// List string arguments
	{
		wxString argDump;
		
		bool preQueryNote = report.url_query().isSet();
		
		bool first = true;
		
		if (preQueryNote)
			for (auto i = report.contents().begin(); i != report.contents().end(); ++i)
		{
			if (i->type != PARAM_STRING) continue;
			
			if (!i->preQuery) continue;
			
			if (first)
			{
				wxString queryLabel = (report.connectionWarning ?
					wxT("We tried to use this info to check for solutions:") :
					wxT("We already used this info to check for solutions:"));
					
				sizerTop->Add(
					new wxStaticText(this, -1, queryLabel),
					0, wxALL | wxALIGN_LEFT, MARGIN);
				
				first = false;
			}
			else argDump += wxT("\n");
			
			// Dump normal arguments
			ContentDump(argDump, &*i);
		}
		
		if (first) preQueryNote = false;
		else
		{
			wxTextCtrl *queryDisplay = new wxTextCtrl(this, -1, argDump,
				wxDefaultPosition, wxSize(450, 125),
				wxTE_MULTILINE|wxTE_READONLY|wxHSCROLL, wxDefaultValidator);
			
			queryDisplay->SetFont(fontTechnical);
			
			//queryDisplay->SetBackgroundStyle( wxBG_STYLE_COLOUR );
			//queryDisplay->SetBackgroundColour( *wxLIGHT_GREY );
			
			sizerTop->Add(queryDisplay, 0, wxEXPAND | wxALL, MARGIN);
			
			// Horizontal rule
			sizerTop->Add(new wxStaticLine(this), 0, wxEXPAND | wxALL, MARGIN);
			
			argDump.Clear();
		}
		
		first = true;
		
		for (auto i = report.contents().begin(); i != report.contents().end(); ++i)
		{
			if (i->type != PARAM_STRING) continue;
			
			if (preQueryNote && i->preQuery) continue;
			
			if (first)
			{
				wxString queryLabel = wxT("The full report below is only sent if you choose");
				if (uiConfig.labelSend().length())
				{
					queryLabel.append(wxT(" `"));
					queryLabel.append(uiConfig.labelSend());
					queryLabel.append(wxT("'"));
				}
				queryLabel.append(wxT(":"));
					
				sizerTop->Add(
					new wxStaticText(this, -1, queryLabel),
					0, wxALL | wxALIGN_LEFT , MARGIN);
				
				first = false;
			}
			else argDump += wxT("\n");
			
			// Dump normal arguments
			ContentDump(argDump, &*i);
		}
		
		if (!first)
		{
			wxTextCtrl *argDisplay = new wxTextCtrl(this, -1, argDump,
				wxDefaultPosition, wxSize(450, 150),
				wxTE_MULTILINE|wxTE_READONLY|wxHSCROLL, wxDefaultValidator);
			
			argDisplay->SetFont(fontTechnical);
			
			//argDisplay->SetBackgroundStyle( wxBG_STYLE_COLOUR );
			//argDisplay->SetBackgroundColour( *wxLIGHT_GREY );
			
			sizerTop->Add(argDisplay, 0, wxEXPAND | wxALL, MARGIN);
		}
	}
	
	// List attached files
	{
	#if 0 //wxMAJOR_VERSION >= 3
		wxWrapSizer *sizerFiles = NULL;
	#else
		wxFlexGridSizer *sizerFiles = NULL;
	#endif
		
		for (auto i = report.contents().begin(); i != report.contents().end(); ++i)
		{
			if (i->type != PARAM_FILE) continue;
			
			wxString shortName = i->path();
			{
				size_t p = shortName.find_last_of("\\/");
				if (p != wxString::npos) shortName = shortName.substr(p+1);
			}
			
			if (!sizerFiles)
			{
			#if 0 //wxMAJOR_VERSION >= 3
				sizerFiles = new wxWrapSizer();
			#else
				sizerFiles = new wxFlexGridSizer(4);
			#endif
			
				/*sizerFiles->Add(
					new wxStaticText(this, -1,
					wxT("Attached files:")),
					0, wxALL | wxALIGN_LEFT, MARGIN);*/
			
				sizerTop->Add(sizerFiles, 0, wxALL | wxALIGN_CENTER, 0);
			}

			wxButton *button = new wxButton(this, wxID_FILE, shortName,
				wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, i->name);
			button->SetBitmap(wxArtProvider::GetBitmap(wxART_NORMAL_FILE, wxART_BUTTON));
			
			sizerFiles->Add(button, 0, wxALL | wxALIGN_CENTER, MARGIN);
		}
	}
	
	// Horizontal rule
	sizerTop->Add(new wxStaticLine(this), 0, wxEXPAND | wxALL, MARGIN);
	
	{
		wxBoxSizer *actionRow = new wxBoxSizer(wxHORIZONTAL);
		
		if (report.path_reviewData().length())
		{
			wxButton *butDir = new wxButton(this, wxID_OPEN);
			butDir->SetBitmap(wxArtProvider::GetBitmap(wxART_FOLDER, wxART_BUTTON));
			actionRow->Add(butDir, 0, wxALL, MARGIN);
		}

		wxButton *butDone = new wxButton(this, wxID_OK);
		actionRow->Add(butDone, 0, wxALL, MARGIN);
		
		butDone->SetDefault();
		
		sizerTop->Add(actionRow, 0, wxALIGN_RIGHT | wxALL);
	}
	
	sizerTop->AddSpacer(MARGIN);

	SetSizerAndFit(sizerTop);

	//Center(wxBOTH);
}

void ViewReport::OnDone (wxCommandEvent & event)
{
	Destroy();
}

void ViewReport::OnClose(wxCloseEvent   & event)
{
	Destroy();
}

void ViewReport::OnOpenFile(wxCommandEvent & event)
{
	wxWindow *window = dynamic_cast<wxWindow*>(event.GetEventObject());
	
	if (window)
	{
		if (auto *content = report.findContent(window->GetName()))
			wxLaunchDefaultApplication(content->path());
	}
}

void ViewReport::OnOpenDir(wxCommandEvent & event)
{
	wxLaunchDefaultApplication(report.path_reviewData());
/*#if __WXMSW__
	wxExecute( wxT("start \"")+report.viewPath+wxT("\""), wxEXEC_ASYNC, NULL );
#else
	wxExecute( wxT("open \"")+report.viewPath+wxT("\""), wxEXEC_ASYNC, NULL );
#endif*/
}

