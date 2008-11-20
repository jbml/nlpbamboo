#include "ycake_hash.hxx"
#include "ycake_mmap.hxx"
#include "bamboo.hxx"
#include "lexicon_factory.hxx"
#include "config_factory.hxx"
#include "ilexicon.hxx"
#include "datrie.hxx"

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <libgen.h>
#include <unistd.h>

#include <map>
#include <list>
#include <vector>
#include <cmath>
#include <iostream>

namespace bamboo { namespace ycake {

class WordAffTrain {
protected:
	bamboo::Parser *_parser;
	bamboo::ILexicon * _token_id_dict;
	std::map<int, std::map<int, int> > _token_aff;
	std::map<int, int> _token_df;
	std::map<int, double> _aff_norm_factor;
	std::map<int, std::string> _token_str;
	int _freq_threshold;
	int _N;
	int _top_relation;
	IConfig *_config;
	int _verbose;

public:
	WordAffTrain(const char * cfg) {
		const char * s;
		_config = ConfigFactory::create(cfg);

		_config->get_value("bamboo_cfg", s);
		_parser = new Parser(s);

		_config->get_value("token_id_dict", s);
		_token_id_dict = LexiconFactory::load(s);

		_config->get_value("freq_threshold", _freq_threshold);
		_config->get_value("word_top_relation", _top_relation);
		_config->get_value("verbose", _verbose);
		_N = 0;
	}

	~WordAffTrain() {
		delete _token_id_dict;
		delete _parser;
		delete _config;
	}

	template <class T>
	struct pair_cmp {
		bool operator() (const std::pair<int, T> &l,
			const std::pair<int, T> &r){
			return l.second > r.second;
		}
	};

	int parse_file(const char * file) {
		std::vector<bamboo::Token> tokens;
		std::map<int, int> doc_token_map;
		MMap mmap;
		mmap.open(file);
		char * buf = (char *)malloc(mmap.getlen() + 1);
		memcpy(buf, mmap.ptr(), mmap.getlen());
		buf[mmap.getlen()] = 0;
		_parser->parse(tokens, buf);
		int i, j, len = tokens.size();
		for(i=0; i<len; ++i) {
			const char * t = tokens[i].token;
			int id = _token_id_dict->search(t);
			if(id>0) {
				doc_token_map[id] += 1;
				_N++;

				std::map<int, std::string>::iterator p
					= _token_str.lower_bound(id);
				if(p==_token_str.end() || p->first!=id) {
					_token_str.insert(p,
						std::make_pair(id, t));
				}
			}
		}

		std::vector<std::pair<int, int> > token_vec;

		std::copy(doc_token_map.begin(), doc_token_map.end(), back_inserter(token_vec));
		int top = doc_token_map.size()/2;
		if(top > _top_relation * 2) top = _top_relation * 2;
		std::partial_sort(token_vec.begin(), token_vec.begin() + top, token_vec.end(), pair_cmp<int>());

		for(i=0; i<top; ++i) {
			int id1 = token_vec[i].first, freq1 = token_vec[i].second;
			_token_df[id1] += 1;
			for(j=i+1; j<top; ++j) {
				int id2 = token_vec[j].first, freq2 = token_vec[j].second;
				int min = freq1<freq2 ? freq1:freq2;
				if(min < _freq_threshold || id1 == id2) continue;
				_token_aff[id1][id2] += min;
				_token_aff[id2][id1] += min;
			}
		}

		free(buf);
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
				std::cerr << "precessed " << i << " files" << std::endl;

			parse_file(it->c_str());
		}

		if(_verbose) 
			std::cerr<<"processed " << i << " files total." << std::endl;

		return 0;
	}

	int stat_aff() {
		double log_n = log(_N);
		std::map<int, std::map<int, int> >::iterator i;
		std::map<int, int>::iterator j;
		std::vector<std::pair<int, double> > aff_vec;

		int max_id = _token_id_dict->max_value() + 1;
		std::vector<std::pair<int, std::pair<int, double> > > all_affinity;

		for(i=_token_aff.begin(); i!=_token_aff.end(); ++i) {
			int id1 = i->first;
			double log_id1 = log(_token_df[id1]);
			aff_vec.clear();

			for(j=i->second.begin(); j!=i->second.end(); ++j) {
				int id2 = j->first, coocc = j->second;
				double aff = 2*log_n + log(coocc) - log_id1 - log(_token_df[id2]);
				aff_vec.push_back(std::make_pair(id2, aff));
			}

			int top = _top_relation < (int)aff_vec.size() ? _top_relation : aff_vec.size();
			std::partial_sort(aff_vec.begin(), aff_vec.begin() + top, aff_vec.end(), pair_cmp<double>());

			int p = 0; double norm_factor = 0;

			for(; p<top; ++p) norm_factor += aff_vec[p].second;
			_aff_norm_factor[id1] += norm_factor;

			for(p=0; p<top; ++p) {
				double aff = 0;
				if(norm_factor>0) aff = aff_vec[p].second/norm_factor;
				all_affinity.push_back(std::make_pair(aff_vec[p].first, std::make_pair(id1, aff)));
			}
		}

		const char * file;
		_config->get_value("token_aff_dict", file);
		int t, hash_size = (int)((float)all_affinity.size() * 1.5);
		YCHash<double> * hash = new YCHash<double>(file, hash_size);
		for(t=0; t<(int)all_affinity.size(); ++t) {
			int id1 = all_affinity[t].first;
			int id2 = all_affinity[t].second.first;
			double aff = all_affinity[t].second.second;
			const char * key1 = _token_str[id1].c_str(), * key2 = _token_str[id2].c_str();
			long long uniq_id = (long long)id1 * (long long)max_id + (long long)id2;
			hash->insert(key1, key2, uniq_id, aff);
		}
		delete hash;
		return 0;
	}

protected:
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

}} //end of namespace bamboo::ycake

using namespace bamboo::ycake;

#define USAGE "ycake_wordaff_train -c config -s corpus_dir"

int main(int argc, char * argv[]) {
	const char * ycake_cfg = NULL, * corpus_dir = NULL;

	int opt;
	while( (opt=getopt(argc, argv, "c:s:")) != -1) {
		switch(opt) {
		case 'c':
			ycake_cfg = optarg;
			break;
		case 's':
			corpus_dir = optarg;
			break;
		case '?':
			printf("%s\n", USAGE);
			exit(1);
		}
	}

	if(!ycake_cfg || !corpus_dir) {
		printf("%s\n", USAGE);
		exit(1);
	}

	WordAffTrain a(ycake_cfg);
	a.parse_dir(corpus_dir);
	a.stat_aff();
	return 0;
}
