//
//  report_http.cpp
//  tattle
//
//  Created by Evan Balster on 11/24/16.
//

#include <iostream>
#include <algorithm>

#include "tattle.h"

#include <wx/progdlg.h>
#include <wx/sstream.h>
#include <wx/uri.h>
#include <wx/frame.h>

#include <wx/msw/winundef.h>
#undef ERROR
#undef DELETE
#include <telling/msg_writer.h>

using namespace tattle;
using namespace std::string_view_literals;

bool Report::ParsedURL::set(wxString fullURL)
{
	url = nng::url(fullURL.c_str());
	
	bool ok = true;

	if (std::string_view(url.get()->u_scheme) == "http"sv) {
		url.get()->u_scheme = "https";
	}
	if (std::string_view(url.get()->u_scheme) != "https"sv)
	{
		std::cout << "Error: URL is not HTTP: " << url.get()->u_scheme << std::endl;
		ok = false;
	}

	if (!url.get()->u_host)
	{
		std::cout << "Error: URL does not specify a host." << std::endl;
		ok = false;
	}
	
	host = url.get()->u_host;
	path = url.get()->u_path;
	port = 80;
	
	// Optionally parse a port
	if (url.get()->u_port)
	{
		port = atol(url.get()->u_port);
		if (!port)
		{
			std::cout << "Error: invalid port `" << url.get()->u_port << "'" << std::endl;
			ok = false;
		}
	}
	
	if (url.get()->u_fragment) std::cout << "Warning: ignoring URL fragment: " << url.get()->u_fragment << std::endl;
	if (url.get()->u_userinfo) std::cout << "Warning: ignoring URL userinfo: " << url.get()->u_userinfo << std::endl;
	if (url.get()->u_query   ) std::cout << "Warning: ignoring URL query: "    << url.get()->u_query    << std::endl;

	return ok;
}


Report::Reply::Reply() :
	connected(false), statusCode(0), error(wxPROTO_NOERR), command(SC_NONE), icon("")
{
}

bool Report::Reply::ok()       const
{
	return connected && error == wxPROTO_NOERR;
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

/*void Report::Reply::connect(HTTPClient &http, const ParsedURL &url)
{
	msgwriter = telling::HttpRequest(url.url.get()->u_path);
	msgwriter.writeHeader("Host", url.host.utf8_string);
	msgwriter.writeHeader_Length();
	auto output = msgwriter.writeBody(); // blank body for this connection test

	auto reply = http.request(msgwriter.release());
	reply.wait(); // TODO: wait for timeout?

	try {
		if (reply.get()) connected = true; // assuming we got a reply, we are now able to connect to the server
	}
	catch (const std::exception& e) {
		std::cout << "Tattle: failed connection to " << url.host << " (error " << error << ")" << std::endl;
		statusCode = 0;
		connected = false;
	}
}*/

void Report::Reply::pull(HTTPClient &http, const ParsedURL &url, bool isQuery, wxString query)
{
	std::cout << "Tattle: connecting to " << url.host << std::endl;

	wxString path = url.path;
	if (!path.Length()) path = wxT("/");

	if (query.Length() && query[0] != wxT('?')) query = wxT("?") + query;

	auto writer = telling::HttpRequest(path.utf8_string() + query.utf8_string(), telling::MethodCode::POST);
	writer.writeHeader("Host", url.host.utf8_string());
	writer.writeHeader("Connection", "close");

	// Boundary with 12-digit random number
	std::string boundary = "tattle-boundary-";
	for (unsigned i = 0; i < 12; ++i) boundary.push_back('0' + (std::rand() % 10));
	writer.writeHeader("Content-Type", "multipart/form-data;boundary=\"" + boundary + "\"");

	// Write header length after other headers are added.
	writer.writeHeader_Length();

	// Create the body of the message; stream into msgwriter's body and use the boundary created above
	report.encodePost(writer.writeBody(), boundary, false);

	// For debugging
	auto msg = writer.release();
	telling::MsgView view(msg);
	std::cout << std::endl << view.bodyString() << std::endl;

	// Release the message and await a reply
	auto replyFuture = http.request(std::move(msg));
	replyFuture.wait();

	std::cout << "Tattle: query `" << (path + query) << ": ";
	raw = wxT("");
	try {
		auto replymsg = replyFuture.get();
		auto view = telling::MsgView(replymsg);
		if (view.status().isSuccess()) {
			std::cout << "success... reply:" << std::endl;
			std::cout << view.bodyString() << std::endl;
			wxStringOutputStream out_stream(&raw);
			raw = wxString(std::string(view.bodyString()));
		}
		else {
			std::cout << "failure... error: " << view.statusString() << std::endl;
		}
	}
	catch (nng::exception& e) {
		std::cout << "failure... exception: " << e.what() << ", context: " << e.who() << std::endl;
	}
	
	// Parse raw reply string into bits
	parseRaw(url);
}

void Report::httpAction(HTTPClient &http, const ParsedURL &url, Reply &reply, wxProgressDialog *prog, bool isQuery) const
{
	if (prog)
	{
		// Set icon and show
		prog->SetIcon(wxArtProvider::GetIcon(uiConfig.defaultIcon));
		prog->Show();
		prog->Raise();
	}

	//encodePost(http, isQuery);

	do
	{
		// Connect to server
		if (prog)
		{
			prog->Update(10, "Connecting to " + url.host + "...");
			wxYield();
		}
		/*reply.connect(http, url);

		//wxSleep(1);  For UI testing

		if (!reply.connected) break;*/

		// Post and download reply
		if (uiConfig.showProgress)
		{
			if (isQuery)
				prog->Update(30, "Talking with " + url.host + "...");
			else
				prog->Update(25, "Sending to " + url.host + "...\nThis may take a while.");
			wxYield();
		}
		reply.pull(http, url, isQuery);

		//wxSleep(1);  For UI testing

		if (uiConfig.showProgress) prog->Update(60);
	}
	while (false);
	
	//http.Close();
}

Report::Reply Report::httpQuery(wxWindow *parent) const
{
	//wxString query = preQueryString();
	
	if (uiConfig.showProgress && parent && uiConfig.stayOnTop)
	{
		// Hack for ordering issue
		parent->Hide();
		parent = NULL;
	}

	Reply reply;
	HTTPClient client(nng::url(queryURL.url));

	if (uiConfig.showProgress)
	{
		wxProgressDialog dialog("Looking for solutions...", "Preparing...", 60, parent,
			wxPD_APP_MODAL | wxPD_AUTO_HIDE | uiConfig.style());

		httpAction(client, queryURL, reply, &dialog, true);
	}
	else
	{
		httpAction(client, queryURL, reply, NULL, true);
	}

	// Ordering issue hack
	if (uiConfig.showProgress && parent && uiConfig.stayOnTop) parent->Show();
	
	return reply;
}

Report::Reply Report::httpPost(wxWindow *parent) const
{
	if (uiConfig.showProgress && parent && uiConfig.stayOnTop)
	{
		// Hack for ordering issue on Mac
		parent->Hide();
		parent = NULL;
	}

	Reply reply;
	HTTPClient client(nng::url(postURL.url));

	if (uiConfig.showProgress)
	{
		wxProgressDialog dialog("Sending...", "Preparing Report...", 60, parent,
			wxPD_APP_MODAL | wxPD_AUTO_HIDE | uiConfig.style());

		httpAction(client, postURL, reply, &dialog, false);
	}
	else
	{
		httpAction(client, postURL, reply, NULL, false);
	}
	
	
	return reply;
}

bool Report::httpTest(const ParsedURL &url) const
{
	//wxHTTP http; http.SetTimeout(5);
	HTTPClient client(nng::url(url.url));

	auto writer = telling::HttpRequest(url.path.utf8_string(), telling::MethodCode::POST);
	writer.writeHeader("Host", url.host.utf8_string());
	writer.writeHeader("Connection", "close");
	writer.writeHeader_Length();
	report.encodePost(writer.writeBody(), "", false);

	auto replyFuture = client.request(writer.release());
	replyFuture.wait();

	bool connected = false;
	try {
		if (replyFuture.get()) {
			connected = true;
		}
	}
	catch (nng::exception& e) {
		std::cout << "http test failed... exception: " << e.what() << ", context: " << e.who() << std::endl;
	}
	
	return connected;
}
