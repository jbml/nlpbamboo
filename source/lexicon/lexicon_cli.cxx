#include <getopt.h>
#include <iostream>
#include <stdexcept>

#include "lexicon_factory.hxx"

static void _info(const char *index)
{
	ILexicon *dc;

	assert(index);
	dc = LexiconFactory::load(index);

	std::cout << " max = " << dc->max_value() 
			  << " min = " << dc->min_value()
			  << " sum = " << dc->sum_value()
			  << std::endl;
}

static void _query(const char *index, const char *query)
{
	ILexicon *dc;

	assert(query); assert(index);
	dc = LexiconFactory::load(index);

	std::cout << query << " = " << dc->search(query) << std::endl;
}

static void _dump(const char *index, const char *target)
{
	ILexicon *dc;

	assert(target); assert(index);
	dc = LexiconFactory::load(index);
	if (dc == NULL) throw std::runtime_error("can not load lexicon");
	dc->write_to_text(target);
}

static void _build(const char *source, const char *index, const char *type)
{
	ILexicon *dc;

	assert(source); assert(index);
	dc = LexiconFactory::create(type);
	if (dc == NULL) throw std::runtime_error("can not create lexicon");
	dc->read_from_text(source, true);
	dc->save(index);
}

static void _help_message()
{
	std::cout << "Usage: lexicon [OPTIONS]\n"
				 "OPTIONS:\n"
				 "        -h|--help             help message\n"
				 "        -i|--index            index file\n"
				 "        -s|--source           source file\n"
				 "        -b|--build            build index, needs -i and -s\n"
				 "        -d|--dump             dump index, needs -i\n"
				 "        -q|--query QUERY      query index, needs -i\n"
				 "        -t|--type TYPE        index type, default = DATrie\n"
				 "        -n|--info             index information, needs -i\n"
				 "\n"
				 "Report bugs to jianing.yang@alibaba-inc.com\n"
			  << std::endl;
}

int main(int argc, char *argv[])
{
	int c;
	const char default_type[] = "datrie";
	const char *index = NULL, *source = NULL, *query = NULL, *type = default_type, *dump = NULL;
	enum action_t {
		ACTION_NO = 0,
		ACTION_BUILD = 1,
		ACTION_DUMP = 2,
		ACTION_QUERY = 3,
		ACTION_INFO = 4
	} action = ACTION_NO;
	
	while (true) {
		static struct option long_options[] =
		{
			{"help", no_argument, 0, 'h'},
			{"build", no_argument, 0, 'b'},
			{"dump", required_argument, 0, 'd'},
			{"query", required_argument, 0, 'q'},
			{"index", required_argument, 0, 'i'},
			{"source", required_argument, 0, 's'},
			{"type", required_argument, 0, 't'},
			{"info", no_argument, 0, 'n'},
			{0, 0, 0, 0}
		};
		int option_index;
		
		c = getopt_long(argc, argv, "hbd:q:i:s:t:n", long_options, &option_index);
		if (c == -1) break;

		switch(c) {
			case 'h':
				_help_message();
				return 0;
			case 'b':
				action = ACTION_BUILD;
				break;
			case 'd':
				action = ACTION_DUMP;
				dump = optarg;
				break;
			case 'n':
				action = ACTION_INFO;
				break;
			case 'q':
				action = ACTION_QUERY;
				query = optarg;
				break;
			case 'i':
				index = optarg;
				break;
			case 's':
				source = optarg;
				break;
			case 't':
				type = optarg;
				break;
		}

	}

	if (action == ACTION_BUILD && index && source && type) {
		_build(source, index, type);
	} else if (action == ACTION_QUERY && index && query) {
		_query(index, query);
	} else if (action == ACTION_DUMP && index && dump) {
		_dump(index, dump);
	} else if (action == ACTION_INFO && index) {
		_info(index);
	} else {
		_help_message();
	}

	return 0;
}
