/*
   Copyright (C) 2000-2007 MySQL AB
   Use is subject to license terms

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING. If not, write to the
   Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
   MA  02110-1301  USA.
*/

/*  The factory header defines an Object Factory, used by SSL message and
 *  handshake types.
 *
 *  See Desgin Pattern in GoF and Alexandrescu's chapter in Modern C++ Design,
 *  page 208
 */



#ifndef yaSSL_FACTORY_HPP
#define yaSSL_FACTORY_HPP

#include STL_VECTOR_FILE
#include STL_PAIR_FILE


namespace STL = STL_NAMESPACE;





namespace yaSSL {


// Factory uses its callback map to create objects by id,
// returning an abstract base pointer
template<class    AbstractProduct, 
         typename IdentifierType = int, 
         typename ProductCreator = AbstractProduct* (*)()
        >
class Factory {                                             
    typedef STL::pair<IdentifierType, ProductCreator> CallBack;
    typedef STL::vector<CallBack> CallBackVector;

    CallBackVector callbacks_;
public:
    // pass function pointer to register all callbacks upon creation
    explicit Factory(void (*init)(Factory<AbstractProduct, IdentifierType,
                                  ProductCreator>&))
    { 
        init(*this); 
    }

    // reserve place in vector before registering, used by init funcion
    void Reserve(size_t sz)
    {
        callbacks_.reserve(sz);
    }

    // register callback
    void Register(const IdentifierType& id, ProductCreator pc)
    {
        callbacks_.push_back(STL::make_pair(id, pc));
    }

    // THE Creator, returns a new object of the proper type or 0
    AbstractProduct* CreateObject(const IdentifierType& id) const
    {
        typedef typename STL::vector<CallBack>::const_iterator cIter;
        
        cIter first = callbacks_.begin();
        cIter last  = callbacks_.end();

        while (first != last) {
            if (first->first == id)
                break;
            ++first;
        }

        if (first == callbacks_.end())
            return 0;
        return (first->second)();
    }
private:
    Factory(const Factory&);            // hide copy
    Factory& operator=(const Factory&); // and assign
};


} // naemspace

#endif // yaSSL_FACTORY_HPP
