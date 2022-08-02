//
//  report_http.cpp
//  tattle
//
//  Created by Evan Balster on 11/24/16.
//

#include <wx/defs.h>

#include <iostream>
#include <algorithm>
#include <future>
#include <chrono>

#include "tattle.h"

#include <wx/progdlg.h>
#include <wx/sstream.h>
#include <wx/uri.h>
#include <wx/frame.h>
#include <wx/mstream.h>


using namespace tattle;



wxWebRequest::State run_request_with_timeout(wxEvtHandler &handler, wxWebRequest &request, int timeout_seconds)
{
	//request.DisablePeerVerify(); // TODO make this configurable?

	if (!request.IsOk())
	{
		std::cout << "Failed to set up Web Request." << std::endl;
		return wxWebRequest::State_Failed;
	}

	std::promise<wxWebRequest::State> promise_state;
	auto future_state = promise_state.get_future();

	wxWebRequest::State result = wxWebRequest::State_Idle;

	// For some reason this isn't working.
	auto eventHandler = [&](wxWebRequestEvent &event)
	{
		if (event.GetEventType() == wxEVT_WEBREQUEST_DATA)
		{
			// meh
		}
		else if (event.GetEventType() == wxEVT_WEBREQUEST_STATE)
		{
			switch (event.GetState())
			{
			case wxWebRequest::State_Completed:
			case wxWebRequest::State_Failed:
			case wxWebRequest::State_Cancelled:
				promise_state.set_value(event.GetState());
				break;
			}
		}
	};

	handler.Bind(wxEVT_WEBREQUEST_STATE, eventHandler);
	handler.Bind(wxEVT_WEBREQUEST_DATA, eventHandler);


	auto time_started = std::chrono::steady_clock::now();
	request.Start();

	size_t bytes_sent = 0, bytes_to_send = 0,
		bytes_recv = 0, bytes_to_recv = 0;

	// Await the completion of the request...
	for (unsigned i = 1; i <= 2*timeout_seconds+1; ++i)
	{
		if (i > timeout_seconds)
		{
			result = wxWebRequest::State_Cancelled; // in case of bad behavior
			request.Cancel();
		}

		if (future_state.wait_until(time_started + std::chrono::seconds(i))
			!= std::future_status::timeout)
			return future_state.get();

		// HACK: if the event handler doesn't work, this will
		auto requestState = request.GetState();
		switch (requestState)
		{
		case wxWebRequest::State_Completed: 
		case wxWebRequest::State_Failed: 
		case wxWebRequest::State_Cancelled:
			result = requestState;
			i = 3*timeout_seconds; // break the outer loop
			break;
		}

		// diagnostics / progress bar
		bytes_sent = request.GetBytesSent();
		bytes_recv = request.GetBytesReceived();
		bytes_to_send = request.GetBytesExpectedToSend();
		bytes_to_recv = request.GetBytesExpectedToReceive();
	}

	handler.Unbind(wxEVT_WEBREQUEST_STATE, eventHandler);
	handler.Unbind(wxEVT_WEBREQUEST_DATA, eventHandler);

	return result;
}



bool Report::ParsedURL::set(wxString url)
{
	wxURI uri(url);
	
	bool ok = true;

	if (uri.HasScheme()) scheme = uri.GetScheme();
	else scheme = "https";

	scheme.MakeLower();

	/*if (uri.HasScheme() && uri.GetScheme() != "http")
	{
		std::cout << "Error: URL is not HTTP." << std::endl;
		ok = false;
	}*/
	
	if (!uri.HasServer())
	{
		std::cout << "Error: URL does not specify a host." << std::endl;
		ok = false;
	}
	
	host = uri.GetServer();
	path = uri.GetPath();
	port = (scheme == "https") ? 443 : 80;
	
	// Optionally parse a port
	if (uri.HasPort())
	{
		if (!uri.GetPort().ToULong(&port))
		{
			std::cout << "Error: invalid port `" << uri.GetPort() << "'" << std::endl;
			ok = false;
		}
	}
	
	if (uri.HasFragment()) std::cout << "Warning: ignoring URL fragment: " << uri.GetFragment() << std::endl;
	if (uri.HasUserInfo()) std::cout << "Warning: ignoring URL userinfo: " << uri.GetUserInfo() << std::endl;
	if (uri.HasQuery()   ) std::cout << "Warning: ignoring URL query: "    << uri.GetQuery()    << std::endl;

	return ok;
}


Report::Reply::Reply() :
	statusCode(0), requestState(wxWebRequest::State_Idle), command(SC_NONE), icon("")
{
}

bool Report::Reply::ok()       const
{
	switch (requestState)
	{
	case wxWebRequest::State_Idle:
	case wxWebRequest::State_Active:
		return true; // ?
	case wxWebRequest::State_Completed:
		return true; // ?
	default:
		return false;
	}
}
bool Report::Reply::valid()    const
{
	return ok() && (message.Length() || title.Length() || link.Length() || command != SC_NONE);
}
bool Report::Reply::sentLink() const
{
	return ok() && link.Length();
}

wxString tattle::GetTagContents(const wxString &reply, const wxString &tagName)
{
	wxString
		tagOpen  = wxT("<") +tagName+wxT(">"),
		tagClose = wxT("</")+tagName+wxT(">");
	
	int
		ptOpen = reply.Find(tagOpen),
		ptClose = reply.Find(tagClose);
	
	if (ptOpen != wxNOT_FOUND && ptClose != wxNOT_FOUND)
	{
		ptOpen += tagOpen.Length();
		if (ptOpen <= ptClose)
			return reply.Mid(ptOpen, ptClose-ptOpen);
	}
	
	return wxString();
}

void Report::Reply::parseRaw(const ParsedURL &url)
{
	wxString comm, iconName;
	
	title    = GetTagContents(raw, wxT("tattle-title")),
	message  = GetTagContents(raw, wxT("tattle-message")),
	link     = GetTagContents(raw, wxT("tattle-link"));
	comm     = GetTagContents(raw, wxT("tattle-command"));
	iconName = GetTagContents(raw, wxT("tattle-icon"));

	icon = uiConfig.GetIconID(iconName);
	
	if      (comm == wxT("STOP"))         command = SC_STOP;
	else if (comm == wxT("PROMPT"))       command = SC_PROMPT;
	else if (comm == wxT("STOP-ON-LINK")) command = SC_STOP_ON_LINK;
	else                                  command = SC_NONE;
	
	if (link.Length())
	{
		// Format into an HTTP link based at the upload URL.
		//    Don't allow links to other websites.
		link = wxT("https://") + url.host + "/" + link;
	}
}

void Report::Reply::processResponse(wxWebRequest::State requestState, wxWebResponse &response, const ParsedURL &url, wxString query)
{
	this->requestState = requestState;

	if (response.IsOk())
	{
		statusCode = response.GetStatus();

		std::cout << "Tattle: got response from " << url.host << std::endl;
	
		wxString path = url.path;
		if (!path.Length()) path = wxT("/");

		raw = response.AsString();
		
		std::cout << "Tattle: query `" << (path+query) << ": success";
		{
			std::cout << "... reply:" << std::endl << raw;
		}
		std::cout << std::endl;
	}
	else
	{
		std::cout << "Tattle: failed connection to " << url.host << std::endl;
	
		// Failed to connect...
		raw = wxT("");
	}
	
	// Parse raw reply string into bits
	parseRaw(url);
}

void Report::httpAction(wxEvtHandler &handler, const ParsedURL &url, Reply &reply, wxProgressDialog *prog, bool isQuery) const
{
	if (prog)
	{
		// Set icon and show
		prog->SetIcon(wxArtProvider::GetIcon(uiConfig.defaultIcon));
		prog->Show();
		prog->Raise();
	}

	int request_time_limit = prog ? 45 : 6;

	//  Note: we don't use query strings anymore due to length limits
	//if (query.Length() && query[0] != wxT('?')) query = wxT("?")+query;

	wxString full_url = url.full();

	wxWebRequest webRequest = wxWebSession::GetDefault().CreateRequest(&handler, full_url);

	wxMemoryBuffer postBuffer;
	{
		std::string boundary_id = "tattle-boundary-";
		for (unsigned i = 0; i < 12; ++i) boundary_id.push_back('0' + (std::rand()%10));

		encodePost(postBuffer, boundary_id, isQuery);
		webRequest.SetData(new wxMemoryInputStream(postBuffer.GetData(), postBuffer.GetDataLen()),
			wxT("multipart/form-data; boundary=\"") + wxString(boundary_id) + ("\""));
	}
	

	do
	{
		// Connect to server
		if (prog)
		{
			prog->Update(10, "Connecting to " + url.host + "...");
			wxYield();
		}

		//wxSleep(1);  For UI testing

		// Post and download reply
		if (uiConfig.showProgress)
		{
			if (isQuery)
				prog->Update(30, "Talking with " + url.host + "...");
			else
				prog->Update(25, "Sending to " + url.host + "...\nThis may take a while.");
			wxYield();
		}

		auto finalState = run_request_with_timeout(handler, webRequest, request_time_limit);

		wxWebResponse response = webRequest.GetResponse();
		reply.processResponse(finalState, response, url);

		webRequest.Cancel(); // in case it didn't go through

		//wxSleep(1);  For UI testing

		if (uiConfig.showProgress) prog->Update(60);
	}
	while (false);
}

Report::Reply Report::httpQuery(wxEvtHandler &parent) const
{
	wxWindow *parentWindow = dynamic_cast<wxWindow*>(&parent), *unhideWindow = nullptr;

	// Hack for ordering issue
	if (parentWindow && uiConfig.showProgress && uiConfig.stayOnTop)
		{parentWindow->Hide(); unhideWindow = parentWindow; parentWindow = NULL;}

	Reply reply;

	if (uiConfig.showProgress)
	{
		wxProgressDialog dialog("Looking for solutions...", "Preparing...", 60, parentWindow,
			wxPD_APP_MODAL | wxPD_AUTO_HIDE | uiConfig.style());

		httpAction(parent, queryURL, reply, &dialog, true);
	}
	else
	{
		httpAction(parent, queryURL, reply, NULL, true);
	}

	// Ordering issue hack
	if (unhideWindow) unhideWindow->Show();
	
	return reply;
}

Report::Reply Report::httpPost(wxEvtHandler &parent) const
{
	wxWindow *parentWindow = dynamic_cast<wxWindow*>(&parent), *unhideWindow = nullptr;

	// Hack for ordering issue
	if (parentWindow && uiConfig.showProgress && uiConfig.stayOnTop)
		{parentWindow->Hide(); unhideWindow = parentWindow; parentWindow = NULL;}

	Reply reply;
	//http.SetTimeout(60);

	if (uiConfig.showProgress)
	{
		wxProgressDialog dialog("Sending...", "Preparing Report...", 60, parentWindow,
			wxPD_APP_MODAL | wxPD_AUTO_HIDE | uiConfig.style());

		httpAction(parent, postURL, reply, &dialog, false);
	}
	else
	{
		httpAction(parent, postURL, reply, NULL, false);
	}
	
	// Ordering issue hack
	if (unhideWindow) unhideWindow->Show();
	
	return reply;
}

bool Report::httpTest(wxEvtHandler &parent, const ParsedURL &url) const
{
	wxWebRequest webRequest = wxWebSession::GetDefault().CreateRequest(&parent, url.full());

	auto finalState = run_request_with_timeout(parent, webRequest, 5);

	bool connected = (finalState == wxWebRequest::State_Completed);
	
	return connected;
}
