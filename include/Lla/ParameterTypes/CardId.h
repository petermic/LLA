#ifndef O2_LLA_INC_CARDID_H
#define O2_LLA_INC_CARDID_H

#include <string>
#include <boost/algorithm/string.hpp>
#include "Lla/rocException.h"
#include "Lla/commonException.h"

namespace o2 {
namespace lla {

inline int cardIdFromString(std::string idstr)
{
    boost::erase_all(idstr,"#");
    int id = std::stoi(idstr);
    if(id<0 || id>7) BOOST_THROW_EXCEPTION(AliceO2::roc::ParseException()
                          << AliceO2::Common::ErrorInfo::Message("Card ID out of range"));
    return id;
}

} // namespace lla
} // namespace o2

#endif