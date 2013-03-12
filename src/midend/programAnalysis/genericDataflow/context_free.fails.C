#include <string>
#include <iostream>
#include <boost/xpressive/xpressive.hpp>
#include <boost/xpressive/regex_actions.hpp>
using namespace boost::xpressive;
using namespace std;

// Displays nested results to std::cout with indenting
struct output_nested_results
{
    int tabs_;

    output_nested_results( int tabs = 0 )
        : tabs_( tabs )
    {
    }

    template< typename BidiIterT >
    void operator ()( match_results< BidiIterT > const &what ) const
    {
        // first, do some indenting
        typedef typename std::iterator_traits< BidiIterT >::value_type char_type;
        char_type space_ch = char_type(' ');
        std::fill_n( std::ostream_iterator<char_type>( std::cout ), tabs_ * 4, space_ch );

        // output the match
        ostringstream oss;
        oss << what[0] << '\n';
        cout << oss.str();

        // output any nested matches
        std::for_each(
            what.nested_results().begin(),
            what.nested_results().end(),
            output_nested_results( tabs_ + 1 ) );
    }
};

int main() {
        sregex group, factor, term, expression;

        group       = '(' >> by_ref(expression) >> ')';
        factor      = +_d | group;
        term        = factor >> *(('*' >> factor) | ('/' >> factor));
        expression  = term >> *(('+' >> term) | ('-' >> term));

        smatch what;
        string s("(8*9+(1+2))");
        if(regex_match(s, what, expression)) {
                std::for_each(
                        what.nested_results().begin(),
                        what.nested_results().end(),
                        output_nested_results());
        } else
                cout << "FAIL composer\n";

        return 0;
}
