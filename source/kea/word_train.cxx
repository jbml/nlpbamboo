#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <libgen.h>
#include <unistd.h>

#include <vector>
#include <map>
#include <string>
#include <list>
#include <iostream>

#include "config_factory.hxx"
#include "ilexicon.hxx"
#include "datrie.hxx"
#include "bamboo.hxx"
#include "utf8.hxx"
#include "kea_mmap.hxx"

namespace bamboo { namespace kea {

class WordTrain {
public:
	struct str_cmp {
		bool operator() (const char * l, const char *r) {
			return strcmp(l, r) < 0;
		}
	};

private:
	bamboo::Parser *_parser;

	std::map<std::string, int> _token_map;

	int _verbose;
	int _freq_threshold;
	int _feature_min_length;
	int _feature_min_utf8_length;
	int _df_threshold;
	IConfig *_config;

public:
	WordTrain(const char * cfg)
		:_verbose(0)
	{
		const char * s;
		_config = ConfigFactory::create(cfg);

		_config->get_value("ke_bamboo_cfg", s);
		_parser = new Parser(s);
		
		_config->get_value("ke_freq_threshold", _freq_threshold);
		_config->get_value("ke_feature_min_length", _feature_min_length);
		_config->get_value("ke_feature_min_utf8_length", _feature_min_utf8_length);
		_config->get_value("ke_df_threshold", _df_threshold);
		_config->get_value("verbose", _verbose);
	}
	~WordTrain() {
		delete _parser;
		delete _config;
	}

	int save_token_dict() {
		int id = 1;
		DATrie token_id_dict;
		DATrie token_df_dict;
		std::map<std::string, int, str_cmp>::iterator
			it = _token_map.begin();
		for(; it != _token_map.end(); ++it) {
			const char * t = it->first.c_str();
			int df = it->second;

			if(df < _df_threshold) continue;

			token_id_dict.insert(t, id++);
			token_df_dict.insert(t, df);
		}

		const char * s;
		_config->get_value("ke_token_id_dict", s);
		token_id_dict.save(s);

		_config->get_value("ke_token_df_dict", s);
		token_df_dict.save(s);

		return 0;
	}

	int parse_dir(const char * path) {
		std::list<std::string> file_list;
		_walk_dir(path, file_list);

		int i = 0;
		std::list<std::string>::iterator it
			= file_list.begin();
		for(; it != file_list.end(); ++it) {

			if(_verbose && i++%1000 == 0) 
				std::cerr<<"processed " << i << " files" << std::endl;

			parse_file(it->c_str());
		}

		if(_verbose) 
			std::cerr<<"processed " << i << " files total." << std::endl;

		return 0;
	}

	int parse_file(const char * file) {
		std::vector<bamboo::Token> tokens;
		std::map<const char *, int, str_cmp> doc_token_map;
		std::map<const char *, int, str_cmp>::iterator mit;
		MMap mmap;
		mmap.open(file);
		char * buf = (char *)malloc(mmap.getlen() + 1);
		memcpy(buf, mmap.ptr(), mmap.getlen());
		buf[mmap.getlen()] = 0;
		_parser->parse(tokens, buf);
		size_t i, len = tokens.size();
		for(i=0; i<len; ++i) {
			const char * t = tokens[i].token;
			doc_token_map[t] += 1;
		}
		for(mit=doc_token_map.begin(); mit!=doc_token_map.end(); ++mit) {
			const char * t = mit->first;
			int cnt = mit->second;
			if(_feature_filter(t, cnt)) continue;

			_token_map[t] += 1;
		}
		free(buf);
		return 0;
	}

protected:

	bool _feature_filter(const char * token, int freq) {
		if(freq < _freq_threshold) {
			return true;
		}
		else if(int(utf8::length(token)) < _feature_min_utf8_length) {
			return true;
		} else if((int)strlen(token) < _feature_min_length) {
			return true;
		}
		return false;
	}

	int _walk_dir(const char * dirname, std::list<std::string> &file_list) {
		DIR *dir;
		struct dirent *dp;
		struct stat st;
		int file_cnt = 0;

		if((dir = opendir(dirname)) == NULL) {
			return 0;
		}

		while((dp = readdir(dir)) != NULL) {
			if( *(dp->d_name) == '.' ) {
				continue;
			}
			char * buf = new char[strlen(dirname) + strlen(dp->d_name) + 2 ];
			sprintf(buf, "%s/%s", dirname, dp->d_name);
			stat(buf, &st);

			if(S_ISDIR(st.st_mode)) {
				file_cnt += _walk_dir(buf, file_list);
			} else {
				file_cnt++;
				file_list.push_back(std::string(buf));
			}

			delete [] buf;
		}

		closedir(dir);
		return file_cnt;
	}
};

}}

using namespace bamboo::kea;

#define USAGE "kea_word_train -c config -s corpus_dir"

int main(int argc, char * argv[]) {
	const char * kea_cfg = NULL, * corpus_dir = NULL;

	int opt;
	while( (opt=getopt(argc, argv, "c:s:")) != -1) {
		switch(opt) {
		case 'c':
			kea_cfg = optarg;
			break;
		case 's':
			corpus_dir = optarg;
			break;
		case '?':
			printf("%s\n", USAGE);
			exit(1);
		}
	}

	if(!kea_cfg || !corpus_dir) {
		printf("%s\n", USAGE);
		exit(1);
	}

	WordTrain a(kea_cfg);
	a.parse_dir(corpus_dir);
	a.save_token_dict();

	return 0;
}
