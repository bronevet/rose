#include <string>
#include <iostream>
#include <boost/xpressive/xpressive.hpp>
#include <boost/xpressive/regex_actions.hpp>
using namespace boost::xpressive;
using namespace std;

        sregex composer, analysis, analysisList, compSpec;


struct output_nested_results
{
    int tabs_;
    list<string> analyses;

    output_nested_results( int tabs = 0 )
        : tabs_( tabs )
    {
    }

    template< typename BidiIterT >
    void operator ()( match_results< BidiIterT > const & what )
    {
        // first, do some indenting
        typedef typename std::iterator_traits< BidiIterT >::value_type char_type;
        char_type space_ch = char_type(' ');
        std::fill_n( std::ostream_iterator<char_type>( std::cout ), tabs_ * 4, space_ch );

        // output the match
        //std::cout << what[0] << '\n';
        ostringstream s; s << what[0] << '\n';
        //cout << s.str();
        //cout << what.str() << endl;
        //analyses.push_back(s.str());

        for(smatch::nested_results_type::const_iterator i=what.nested_results().begin(); i!=what.nested_results().end(); i++){
                 { char_type space_ch = char_type(' '); std::fill_n( std::ostream_iterator<char_type>( std::cout ), tabs_ * 4, space_ch ); }
                cout << ":"<<i->str()<<endl;
        }
        // output any nested matches
        output_nested_results o(tabs_ + 1);
        std::for_each(
            what.nested_results().begin(),
            what.nested_results().end(),
            o);
        /*{ char_type space_ch = char_type(' '); std::fill_n( std::ostream_iterator<char_type>( std::cout ), tabs_ * 4, space_ch ); }
        cout << "this="<<this<<" o="<<&o<<" #o.analyses="<<o.analyses.size()<<" #analyses="<<analyses.size()<<" tabs="<<tabs_<<endl;
        for(list<string>::iterator a=o.analyses.begin(); a!=o.analyses.end(); a++) {
                { char_type space_ch = char_type(' '); std::fill_n( std::ostream_iterator<char_type>( std::cout ), tabs_ * 4, space_ch ); }
                cout << *a << endl;
        }*/
    }
};

void composerCreator(string s, string indent) {
        sregex alphaList, alphaParens;
        alphaParens = +(_w | '(' | ')' | ',' | _s);
        alphaList = (s1=+_w) >> *_s >> '(' >> alphaParens >> *(*_s >> ',' >> *_s >> (s2=alphaParens) ) >> *_s >> ')';

        smatch what;
        cout << indent << "s=\""<<s<<"\"\n";
        if(regex_match(s, what, alphaList)) {
                cout << indent << "MATCH, #what="<<what.size()<<" #what.nested_results()="<<what.nested_results().size()<<"\n";
                smatch::iterator m=what.begin();
                cout << indent << "m="<<*m<<endl;
                cout << indent << "m.nest="<<*(what.nested_results().begin())<<endl;
                if(*m == "loosechain") cout << indent << "LC";
                else if(*m == "loosepar") cout << indent << "LP";
                m++;

                for(; m!=what.end(); m++) {
                        string ss = *m;
                        if(regex_match(ss, what, alphaList))
                                composerCreator(ss, indent+"    ");
                        cout << indent << "analysis: "<<*m<<endl;
                }
        } else
                cout << indent << "FAIL\n";
}

int main() {
        composer = as_xpr(icase("loosechain")) | icase("loosepar");
        analysis = as_xpr(icase("constantpropagation")) | icase("livedead");
        analysisList = '(' >> (by_ref(analysis) | by_ref(compSpec)) >> *(*_s >> "," >> *_s >> (by_ref(analysis) | by_ref(compSpec))) >> ')';
        compSpec = by_ref(composer) >> analysisList;

        smatch what;
        string s("loosechain(livedead, loosepar(constantpropagation, livedead), livedead, constantpropagation)");
        if(regex_match(s, what, compSpec)) {
                cout << "MATCH composer\n";
                /*for(match_results<smatch>::iterator m=what.begin(); m!=what.end(); m++) {
                        cout << *m << endl;
/*                      for(int i=0; i<what.nested_results().size(); i++)
                                cout << "    " << what.nested_results()[i]<<endl;* /
                        //for(std::_List_const_iterator<boost::xpressive::match_results<__gnu_cxx::__normal_iterator<const char*, std::basic_string<char, std::char_traits<char>, std::allocator<char> > > > > m2=what.nested_results().begin(); m2!=what.nested_results().end(); m2++) {
                        for(std::_List_const_iterator<boost::xpressive::match_results<__gnu_cxx::__normal_iterator<const char*, std::basic_string<char, std::char_traits<char>, std::allocator<char> > > > > m2=m->nested_results().begin(); m2!=m->nested_results().end(); m2++) {
                                cout << "    " << *m2 << endl;
                        }
                }*/
                std::for_each(
                        what.nested_results().begin(),
                        what.nested_results().end(),
                        output_nested_results());
        } else
                cout << "FAIL composer\n";

        //composerCreator("loosechain(livedead, loosepar(constantpropagation, livedead), livedead, constantpropagation)", "");

        return 0;
}
