#include <sstream>

#include "tattle.h"


using namespace tattle;



PersistentData::PersistentData() :
	data(_data)
{
}

static bool PersistentData_Load(const wxString &path, Json& json)
{
	json = nullptr;

	if (!wxFile::Exists(path)) return false;

	wxFile file(path);
	if (!file.IsOpened()) return false;

	try
	{
		wxString contents_str;
		file.ReadAll(&contents_str);
		file.Close();

		const auto contents_utf8 = contents_str.ToUTF8();

		json = Json::parse(contents_utf8.data(), contents_utf8.data() + contents_utf8.length(), nullptr, true, true);

		return true;
	}
	catch (Json::parse_error& e)
	{
		std::cout << "Failed to read persistent data from `" << path << "' : " << e.what() << std::endl;
		return false;
	}
	catch (...)
	{
		std::cout << "Failed to read persistent data from `" << path << "' : unknown exception" << std::endl;
		return false;
	}
}

static bool PersistentData_Store(const wxString& path, const Json& json)
{
	wxFile file(path, wxFile::OpenMode::write);
	if (!file.IsOpened()) return false;

	file.Write(json.dump(1, '\t', false, nlohmann::detail::error_handler_t::replace));

	file.Close();

	return true;
}

bool PersistentData::load(wxString path)
{
	if (PersistentData_Load(path, _data))
	{
		_path = path;
		return true;
	}
	else
	{
		if (_data.is_null()) _data = Json::object({
			/*{"consent", {}},
			{"cookies", {}},
			{"input", {}},*/
		});
		if (!_path.length()) _path = path;
		return false;
	}
}

bool PersistentData::mergePatch(const Json& patch)
{
	if (!_path.length()) return false;

	// Attempt to load the latest file data; otherwise use what's in memory
	Json data;
	if (!PersistentData_Load(_path, data)) data = this->_data;
	
	// Merge patch and store to file.
	//   merge_patch operation cannot fail, barring an out-of-memory condition.
	data.merge_patch(patch);
	return PersistentData_Store(_path, data);
}
