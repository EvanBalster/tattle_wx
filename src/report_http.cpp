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



wxWebRequest::State run_request_with_timeout(
	wxEvtHandler &handler, wxWebRequest &request, int timeout_seconds,
	wxProgressDialog *progress)
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
	for (long idle_time = 1; idle_time <= 2*timeout_seconds+1; ++idle_time)
	{
		if (idle_time > timeout_seconds)
		{
			result = wxWebRequest::State_Cancelled; // in case of bad behavior
			request.Cancel();
		}

		if (future_state.wait_until(time_started + std::chrono::seconds(idle_time))
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
			idle_time = 3*timeout_seconds; // break the outer loop
			break;
		}

		auto
			send_ct = request.GetBytesSent(),
			send_ex = request.GetBytesExpectedToSend(),
			recv_ct = request.GetBytesReceived(),
			recv_ex = request.GetBytesExpectedToReceive();

		// Reset timeout if transaction is making progress
		if (send_ct != bytes_sent || recv_ct != bytes_recv)
		{
			time_started = std::chrono::steady_clock::now();
			idle_time = 1;
		}

		// diagnostics / progress bar
		bytes_sent = send_ct;
		bytes_recv = recv_ct;
		bytes_to_send = send_ex;
		bytes_to_recv = recv_ex;

		if (progress)
		{
			float pct
				= (80.f*bytes_sent)/bytes_to_send
				+ (20.f*bytes_recv)/bytes_to_recv;
			progress->Update(std::floor(pct));
		}
	}

	handler.Unbind(wxEVT_WEBREQUEST_STATE, eventHandler);
	handler.Unbind(wxEVT_WEBREQUEST_DATA, eventHandler);

	return result;
}



bool Report::ParsedURL::set(std::string url)
{
	return set(wxString::FromUTF8(url.data(), url.length()));
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
	jsonValues = GetTagContents(raw, wxT("tattle-json"));

	identity = Report::Identifier(std::string(GetTagContents(raw, wxT("tattle-id"))));

	icon = uiConfig.GetIconID(iconName);
	
	if      (comm == wxT("STOP"))         command = SC_STOP;
	else if (comm == wxT("PROMPT"))       command = SC_PROMPT;
	else if (comm == wxT("STOP-ON-LINK")) command = SC_STOP_ON_LINK;
	else                                  command = SC_NONE;
	
	if (link.Length())
	{
		// Format into an HTTPS link based at the upload URL.
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
		prog->SetIcon(wxArtProvider::GetIcon(uiConfig.defaultIcon()));
		prog->Show();
		prog->Raise();
	}

	int request_time_limit = prog ? 45 : 6;

	//  Note: we don't use query strings anymore due to length limits
	//if (query.Length() && query[0] != wxT('?')) query = wxT("?")+query;

	wxString full_url = url.full();

	wxWebRequest webRequest = wxWebSession::GetDefault().CreateRequest(&handler, full_url);

	webRequest.SetMethod("POST");

	wxMemoryBuffer postBuffer;
	{
		std::string boundary_id = "tattle-boundary-";
		for (unsigned i = 0; i < 12; ++i) boundary_id.push_back('0' + (std::rand()%10));

		encodePost(postBuffer, boundary_id, isQuery);
		webRequest.SetData(new wxMemoryInputStream(postBuffer.GetData(), postBuffer.GetDataLen()),
			wxT("multipart/form-data; boundary=\"") + wxString(boundary_id) + ("\""),
			postBuffer.GetDataLen());
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
		if (uiConfig.showProgress())
		{
			if (isQuery)
				prog->Update(30, "Talking with " + url.host + "...");
			else
				prog->Update(25, "Sending to " + url.host + "...\nThis may take a while.");
			wxYield();
		}

		auto finalState = run_request_with_timeout(handler, webRequest, request_time_limit, prog);

		wxWebResponse response = webRequest.GetResponse();

		reply.processResponse(finalState, response, url);

		if (report.enable_server_values() && reply.jsonValues.length()) try
		{
			const auto contents_utf8 = reply.jsonValues.ToUTF8();

			Json server_values = Json::parse(contents_utf8.data(), contents_utf8.data() + contents_utf8.length(), nullptr, true, true);

			if (!server_values.is_object()) throw 0;

			Json permitted_values = Json::object();
			for (auto i = server_values.begin(); i != server_values.end(); ++i)
			{
				// Don't allow the server to set keys with a prefix
				if (!i.key().length()) continue;
				switch (i.key()[0])
				{
				case int('$'): case int('.'):
					// Don't allow server to write keys starting with these characters.
					continue;
				}
				permitted_values[i.key()] = std::move(i.value());
			}

			if (permitted_values.size())
				persist.mergePatch(permitted_values);
		}
		catch (Json::parse_error &) {}
		catch (int) {}

		webRequest.Cancel(); // in case it didn't go through

		//wxSleep(1);  For UI testing

		if (uiConfig.showProgress()) prog->Update(100);
	}
	while (false);
}

Report::Reply Report::httpQuery(wxEvtHandler &parent) const
{
	wxWindow *parentWindow = dynamic_cast<wxWindow*>(&parent), *unhideWindow = nullptr;

	// Hack for ordering issue
	if (parentWindow && uiConfig.showProgress() && uiConfig.stayOnTop())
		{parentWindow->Hide(); unhideWindow = parentWindow; parentWindow = NULL;}

	Reply reply;

	if (uiConfig.showProgress())
	{
		wxProgressDialog dialog("Looking for solutions...", "Preparing...", 100, parentWindow,
			wxPD_APP_MODAL | wxPD_AUTO_HIDE | uiConfig.style());

		httpAction(parent, url_query(), reply, &dialog, true);
	}
	else
	{
		httpAction(parent, url_query(), reply, NULL, true);
	}

	// Ordering issue hack
	if (unhideWindow) unhideWindow->Show();
	
	return reply;
}

Report::Reply Report::httpPost(wxEvtHandler &parent) const
{
	wxWindow *parentWindow = dynamic_cast<wxWindow*>(&parent), *unhideWindow = nullptr;

	// Hack for ordering issue
	if (parentWindow && uiConfig.showProgress() && uiConfig.stayOnTop())
		{parentWindow->Hide(); unhideWindow = parentWindow; parentWindow = NULL;}

	Reply reply;
	//http.SetTimeout(60);

	if (uiConfig.showProgress())
	{
		wxProgressDialog dialog("Sending...", "Preparing Report...", 100, parentWindow,
			wxPD_APP_MODAL | wxPD_AUTO_HIDE | uiConfig.style());

		httpAction(parent, url_post(), reply, &dialog, false);
	}
	else
	{
		httpAction(parent, url_post(), reply, NULL, false);
	}
	
	// Ordering issue hack
	//  TODO this causes the prompt window to "blink in" after posting
	if (unhideWindow) unhideWindow->Show();
	
	return reply;
}

bool Report::httpTest(wxEvtHandler &parent, const ParsedURL &url) const
{
	wxWebRequest webRequest = wxWebSession::GetDefault().CreateRequest(&parent, url.full());

	auto finalState = run_request_with_timeout(parent, webRequest, 5, nullptr);

	bool connected = (finalState == wxWebRequest::State_Completed);
	
	return connected;
}
