#pragma once



#include <string>

#include <vector>



#include "save/item_types.hpp"



namespace totk {



class VariableStore;



namespace HorseDefaults {



HorseItem createForId(const std::string& id, const std::vector<HorseItem>& owned, VariableStore* vars);

void finalizeForStableSave(HorseItem& item, const std::vector<HorseItem>& allHorses, VariableStore* vars);

void syncUsedUidRegistry(VariableStore* vars, const std::vector<HorseItem>& horses);



}  // namespace HorseDefaults

}  // namespace totk


