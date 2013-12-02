
#include <future>
#include <iostream>
#include "search_task.hpp"

search_task::search_task(const Input &input) : _input(input), _output() {}

void search_task::do_search() { search(_input.root, true).wait(); }

const Output &search_task::getOutput() { return _output; }

string_pool search_task::output_path(const boost::filesystem::path &input) {
  string_pool result(input.generic_string().c_str());
  for (char &c : result) {
    if (c == '/')
      c = '\\';
  }
  return result;
}

std::vector<pair_stringpool_int, boost::pool_allocator<pair_stringpool_int> >
search_task::search_content(const boost::filesystem::path &path,
                            const std::string &regex) {
  std::vector<pair_stringpool_int, boost::pool_allocator<pair_stringpool_int> >
  result;
  std::ifstream stream(path.c_str());
  if (stream.good()) {
    stream.sync_with_stdio(false);
    string_pool buffer;
    buffer.reserve(128);
    int line = 0;
    while (std::getline(stream, buffer)) {
      if (find(buffer, regex)) {
        int size = buffer.size();
        result.push_back(std::make_pair(std::move(buffer), line));
        buffer.reserve(size);
      }
      line++;
    }
  }
  return result;
}

void search_task::search_file(const boost::filesystem::path &path) {
  if (_input.filename &&
      find(path.filename().native(), _input.regex)) // search on name
  {
    std::lock_guard<std::mutex> g(_output.coutLock);
    std::cout << output_path(path).c_str() << "\r\n";
  }

  if (_input.filterfilename && !find(path.leaf().string(), _input.filterEx))
    return;

  if (_input.content) // search in content
  {
    auto result = search_content(path, _input.regex);
    if (result.size()) {
      std::lock_guard<std::mutex> g(_output.coutLock);
      std::cout << output_path(path) << "\r\n";
      for (auto pair : result) {
        std::cout << "line " << pair.second << " : " << pair.first << "\r\n";
      }
    }
  }
}

void search_task::search_directory(const boost::filesystem::path &path) {
  if (_input.directoryName && find(path.leaf().string(), _input.regex)) {
    std::lock_guard<std::mutex> g(_output.coutLock);
    std::cout << output_path(path) << "\r\n";
  }

  if (_input.filterdirectoryName &&
      !find(path.leaf().string(), _input.filterEx))
    return;

  std::vector< std::future<void> > directory_finished;
  std::for_each(
      boost::filesystem::directory_iterator(path),
      boost::filesystem::directory_iterator(),
      [this, &directory_finished](const boost::filesystem::path &subpath) {
	  directory_finished.push_back(
		  search(std::move(subpath)));
      });
  for (auto &future : directory_finished) {
    future.wait();
  }
  std::cout.flush();
}

std::future< void > search_task::search(boost::filesystem::path path) {
	boost::system::error_code error;
	auto stat = boost::filesystem::status(path, error);
	if (!error) {
		auto type = stat.type();
		if (_input.recursive && type == boost::filesystem::file_type::directory_file) {
			return std::async(&search_task::search_directory, this, std::move(path));
		}
		else if (type == boost::filesystem::file_type::regular_file) {
			return std::async(&search_task::search_file, this, std::move(path));
		}
	}
	return std::async([](){});
}

std::future< void > search_task::search(boost::filesystem::path path, bool recurcive) {
	boost::system::error_code error;
	auto stat = boost::filesystem::status(path, error);
	if (!error) {
		auto type = stat.type();
		if (recurcive && type == boost::filesystem::file_type::directory_file) {
			return std::async(&search_task::search_directory, this, std::move(path));
		}
		else if (type == boost::filesystem::file_type::regular_file) {
			return std::async(&search_task::search_file, this, std::move(path));
		}
	}
	return std::async([](){});
}
