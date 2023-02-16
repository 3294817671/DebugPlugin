#pragma once
#include <ntifs.h>
#include "ia32_type.h"

/// A structure made up of mutual fields across all EPT entry types
union EptCommonEntry {
	ULONG64 all;
	struct {
		ULONG64 read_access : 1;       //!< [0]
		ULONG64 write_access : 1;      //!< [1]
		ULONG64 execute_access : 1;    //!< [2]
		ULONG64 memory_type : 3;       //!< [3:5]
		ULONG64 reserved1 : 6;         //!< [6:11]
		ULONG64 physial_address : 36;  //!< [12:48-1]
		ULONG64 reserved2 : 16;        //!< [48:63]
	} fields;
};
static_assert(sizeof(EptCommonEntry) == 8, "Size check");
// EPT related data stored in ProcessorData
struct EptData {
	EptPointer* ept_pointer;
	EptCommonEntry* ept_pml4;

	EptCommonEntry** preallocated_entries;  // An array of pre-allocated entries
	volatile long preallocated_entries_count;  // # of used pre-allocated entries
};


namespace ept
{
	EXTERN_C{

		/// Checks if the system supports EPT technology sufficient enough
		/// @return true if the system supports EPT
		_IRQL_requires_max_(PASSIVE_LEVEL) bool EptIsEptAvailable();
	/// Reads and stores all MTRRs to set a correct memory type for EPT
	   _IRQL_requires_max_(PASSIVE_LEVEL) void EptInitializeMtrrEntries();
	   ULONG64 EptGetEptPointer(EptData* ept_data);

	   /// De-allocates \a ept_data and all resources referenced in it
       /// @param ept_data   A returned value of EptInitialization()
	   void EptTermination(_In_ EptData* ept_data);



	   /// Returns an EPT entry corresponds to \a physical_address
       /// @param ept_data   EptData to get an EPT entry
       /// @param physical_address   Physical address to get an EPT entry
       /// @return An EPT entry, or nullptr if not allocated yet
	   EptCommonEntry* EptGetEptPtEntry(_In_ EptData* ept_data,
		   _In_ ULONG64 physical_address);


	   _Use_decl_annotations_ EptData* EptInitialization();

	   ///// Handles VM-exit triggered by EPT violation
    //   /// @param ept_data   EptData to get an EPT pointer
	   //_IRQL_requires_min_(DISPATCH_LEVEL) void EptHandleEptViolation(
		  // _In_ EptData* ept_data, _In_ ShadowHookData* sh_data,
		  // _In_ SharedShadowHookData* shared_sh_data);


	   /// Handles VM-exit triggered by EPT violation
/// @param ept_data   EptData to get an EPT pointer
	   _IRQL_requires_min_(DISPATCH_LEVEL) void EptHandleEptViolation(
		   _In_ EptData* ept_data);

	   static bool EptpIsDeviceMemory(_In_ ULONG64 physical_address);
	   
	}
}
EXTERN_C memory_type EptpGetMemoryType(_In_ ULONG64 physical_address);

