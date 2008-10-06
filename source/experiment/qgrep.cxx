/*
 * Copyright (c) 2008, detrox@gmail.com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <organization> nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY <copyright holder> ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <copyright holder> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */

#include <cstdlib>
#include <cstdio>
#include <iostream>
#include "lexicon_factory.hxx"

static int _qgrep(const char *pat, const char *src)
{
	char str[4096];
	TrieLexicon<DATrie> dc(1024);
	FILE *fsrc = NULL, *fpat = NULL;
	int i = 0;

	/* load source */
	if ((fsrc = fopen(src, "r")) == NULL) {
		std::cerr << "can not open source file " << src << std::endl;
		return -1;
	}
	while(1) {
		int cnt;
	
		if (fscanf(fsrc, "%4096s", str) == EOF) break;
		cnt = dc.search(str) + 1;
		dc.insert(str, cnt);
		++i;
		if (i % 10000 == 0)
			std::clog << i << " items processed" << std::endl;
	}
	fclose(fsrc);

	if ((fpat = fopen(pat, "r")) == NULL) {
		std::cerr << "can not open pattern file " << pat << std::endl;
		return -1;
	}
	while(1) {
		int val;
	
		if (fscanf(fpat, "%4096s", str) == EOF) break;
		val = dc.search(str);
		std::cout << str << "\t" << val << std::endl;
	}
	fclose(fpat);

	return 0;
}

int main(int argc, char *argv[])
{
	if (argc < 2 || !argv[1] || !argv[2]) {
		std::cerr << "Usage: qgrep pattern log\n"
				     "\n"
				     "Report bug to jianing.yang@alibaba-inc.com\n"
				  << std::endl;
		return 0;
	}

	return _qgrep(argv[1], argv[2]);
}
