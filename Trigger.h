/*
     Created by Paul Marinescu and George Candea
     Copyright (C) 2009 EPFL (Ecole Polytechnique Federale de Lausanne)

     This file is part of LFI (Library-level Fault Injector).

     LFI is free software: you can redistribute it and/or modify it  
     under the terms of the GNU General Public License as published by the  
     Free Software Foundation, either version 3 of the License, or (at  
     your option) any later version.

     LFI is distributed in the hope that it will be useful, but  
     WITHOUT ANY WARRANTY; without even the implied warranty of  
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU  
     General Public License for more details.

     You should have received a copy of the GNU General Public  
     License along with LFI. If not, see http://www.gnu.org/licenses/.

     EPFL
     Dependable Systems Lab (DSLAB)
     Room 330, Station 14
     1015 Lausanne
     Switzerland
*/

#include <libxml/tree.h>
#include <vector>
#include <string>
#include <map>
#include <memory>

using namespace std;

class Trigger
{
public:
  virtual void Init(xmlNodePtr initData) {}
  virtual bool Eval(const string& functionName, ...) = 0;
};

typedef Trigger* (*FactoryMethod)() ;

class RegEntry ;

class Class
{
public :
  static Class forName( std::string s )
  {
    return( Class( s ) ) ;
  }
  Trigger* newInstance() ;

  static Trigger* newI(std::string s)
  {
    return (Class(s).newInstance());
  }
private :
  std::string name ;
  typedef std::map< std::string, FactoryMethod > FactoryMethodMap ;
  static FactoryMethodMap* fmMap ;
  friend class RegEntry ;
  static void Register( std::string s, FactoryMethod m ) ;
  Class( std::string s ) : name( s ) 
  {
  }
};


class RegEntry 
{
public :
  RegEntry( const char s[], FactoryMethod f ) 
  { 
    Class::Register( s, f ) ;
  }
};


template< class T, const char S[] > class Registered
{
public :
  static Trigger* newInstance() 
  {
    return( new T() ) ;
  }
protected :
  Registered()
  {
    const RegEntry& dummy = r ;
  }
private :
  static const RegEntry r ;
} ;

template< class T, const char S[] > const RegEntry 
Registered< T, S > :: r = RegEntry( S, Registered< T, S >::newInstance ) ;

#define DEFINE_TRIGGER( C ) \
char C##Name__[] = #C ; \
class C : public Registered< C, C##Name__ >, public Trigger
