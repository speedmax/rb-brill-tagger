#include <ctype.h>
#include "tagger.h"
#include <set>
#include <algorithm>
#include <sstream>
#include <iterator>
#include <string>
#include <vector>
#include "porter_stemmer.h"

struct WordComparitor
{
  bool operator()(const std::pair<std::string,int> &s1, const std::pair<std::string,int> &s2) const
  {
    return s1.second < s2.second;
  }
};


// from http://www.thescripts.com/forum/thread167600.html
// split words by ' '
static std::vector<std::string> word_split(const std::string& s)
{
  std::string words = s;
  // convert all non alpha characters to spaces
  for( int i = 0; i < words.length(); ++i ) { 
    if( !isalpha( words[i] ) ) {
      words[i] = ' '; // convert to space
    }
  }

  std::istringstream is(words);
  return std::vector<std::string>(std::istream_iterator<std::string>(is), std::istream_iterator<std::string>());
}

NWordTagger::NWordTagger() 
 : nwords(2), stemmer(porter_stemmer_new()){
}
NWordTagger::~NWordTagger(){
  porter_stemmer_free(stemmer);
}
void NWordTagger::loadTags( const std::set<std::string> &tags )
{
  for( std::set<std::string>::iterator i = tags.begin(); i != tags.end(); ++i ){
    std::string stemmed, word = std::string(*i);
    std::vector<std::string> words = word_split( *i );
    //printf( "word: %s\n", word.c_str() );

    if( words.size() > 1 ){
      for( size_t j = 0; j < words.size(); ++j ){
        stemmed += this->stemWord(words[j]) + " ";
      }
      stemmed = stemmed.substr(0,stemmed.length()-1);
      this->tags[stemmed] = word;
      //printf( "word: %s -> %s\n", word.c_str(), stemmed.c_str() );
    }
    else{
      stemmed = this->stemWord(*i);
      //printf( "word: %s -> %s\n", word.c_str(), stemmed.c_str() );
      this->tags[stemmed] = word;
    }

  }
}
std::string NWordTagger::stemWord( const std::string &word )const
{
  std::string stemmed;
  char *transition_buffer = strdup( word.c_str() );
  stemmed = word.substr(0,porter_stem(this->stemmer, transition_buffer, word.length()-1 )+1);
  free( transition_buffer );
  return stemmed;
}
  
std::vector<std::string> NWordTagger::execute( const char *text, short max )const
{
  int max_count = 0;
  std::vector<std::string> words = word_split( text );
  std::map<std::string, int> matched_tags; // stores tags and frequency
  std::string match_word;
  std::map<std::string,std::string>::const_iterator matched;

  // loop over the words stemming each word
  for( size_t i = 0; i < words.size(); ++i ) {

    // get the stemmed word at position i
    match_word = this->stemWord(words[i]);

    // now scan ahead nwords positions searching our tags table for matches
    for( short j = 1; (j <= this->nwords) && ((i+j) < words.size()); ++j ) {
      matched = this->tags.find( match_word );
      if( matched != this->tags.end() ){
        //printf( "word: %d:(%s->%s)\n", i, match_word.c_str(), matched->second.c_str() );
        std::map<std::string, int>::iterator mloc = matched_tags.find( matched->second );
        if( mloc == matched_tags.end() ) {
          matched_tags[matched->second] = 1; // count 1
        }
        else {
          mloc->second++;
          if( max_count < mloc->second ) { max_count = mloc->second; }
        }
      }
      // stem each word and compare against our tag bank
      //printf( "window: %ld:%lu\n", i,(i+j) );
      match_word += " " + this->stemWord(words[i+j]);
    }

    matched = this->tags.find( match_word );
    if( matched != this->tags.end() ) {
      //printf( "word: %ld:(%s->%s)\n", i, words[i].c_str(), match_word.c_str() );
      std::map<std::string, int>::iterator mloc = matched_tags.find( matched->second );
      if( mloc == matched_tags.end() ) {
        matched_tags[matched->second] = 1; // count 1
      }
      else {
        mloc->second++;
        if( max_count < mloc->second ) { max_count = mloc->second; }
      }
    }
  }

  // now that we have all the matched tags reduce to max using the tag frequency as a reduction measure
  std::vector< std::pair<std::string,int> > sorted_tags;

  //printf( "max frequency: %d\n", max_count );
  for( std::map<std::string, int>::iterator mloc = matched_tags.begin(); mloc != matched_tags.end(); ++mloc ){
    //printf( "word: %s, frequency: %d\n", mloc->first.c_str(), mloc->second ); 
    sorted_tags.push_back(*mloc);
  }

  // sort the tags in frequency order
  std::sort( sorted_tags.begin(), sorted_tags.end(), WordComparitor() );

  std::vector< std::string > reduced_tags;

  std::vector< std::pair<std::string, int> >::iterator mloc;
  do {
    for(mloc = sorted_tags.begin(); mloc != sorted_tags.end(); ++mloc ) {
      std::pair< std::string, int > word_freq = *mloc;
     // printf( "word: %s, frequency: %d\n", word_freq.first.c_str(), word_freq.second );
      //printf( "word: %s, frequency: %d\n", mloc->first.c_str(), mloc->second ); 
      if( word_freq.second < max_count ) { 
        sorted_tags.erase( mloc );
        break;
      }
    }
  } while( sorted_tags.size() > max && mloc != sorted_tags.end() );

  for( size_t i = 0; i < sorted_tags.size(); ++i ) {
    reduced_tags.push_back( sorted_tags[i].first );
  }

  return reduced_tags;
}
