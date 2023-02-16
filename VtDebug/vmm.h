#pragma once
#include <ntifs.h>

/* https://www.cnblogs.com/Rev-omi/p/14063037.html
VMM��Virtual Machine Monitor��������������Ҳ��Ϊ ��Hypervisior������Ȩ��(Ring -1)���ܹ���ز���ϵͳ�ĸ�����Ϊ��
VMX��Virtual Machine Extension���������չ���� CPU �ṩ��һ�ֹ��ܡ�
VMCS��Virtual-Machine Control Structure����������ƽṹ��һ���ڴ�����
APIC��Advanced Programmable Interrupt Controller �߼��ɱ���жϿ�������Ӳ���豸
MSR��Model Specific Register��һ��64λ�Ĵ�����ͨ�� RDMSR �� WRMSR ���ж�д������������ IA32_ Ϊǰ׺��
һϵ�����ڿ��� CPU ���С����ܿ��ء����ԡ����ٳ���ִ�С���� CPU ���ܵȷ���ļĴ���

CR0-CR3:���ƼĴ�����
CR0�����ƴ���������ģʽ��״̬
CR1����������
CR2������ҳ���������Ե�ַ
CR3��ҳĿ¼�������ڴ����ַ


VMXON:���� VMX ģʽ,����ִ�к��������⻯���ָ�
VMXOFF:�ر� VMX ģʽ���������⻯ָ���ִ�ж���ʧ�ܡ�
VMLAUNCH:���� VMCSָ�������� Guest OS��
VMRESUME:�� Hypervisor �лָ������ Guest OS ��ִ�С�
VMPTRLD:����һ�� VMCS,�޸Ĵ�������ǰ VMCS ָ��Ϊ����� VMCS �����ַ��
VMCLEAR:ʹһ�� VMCS ��Ϊ�Ǽ���״̬�����´�������ǰ VMCS ָ��Ϊ�ա�
VMPTRST:�� VMCS �洢��ָ��λ�á�
VMREAD:��ȡ��ǰ VMCS �е����ݡ�
VMWRITE:��ǰ VMCS ��д�����ݡ�
VMCALL:Guest OS �� Hypervisor ����ָ�Guest OS ����� #VMExit ������ Hypervisor��
INVEPT:ʹ TLB �л���ĵ�ַӳ��ʧЧ��
INVVPID:ʹĳ�� VPID ����Ӧ�ĵ�ַӳ��ʧЧ��

*/




/// Represents VMM related data shared across all processors
struct SharedProcessorData {
	volatile long reference_count;  //!< Number of processors sharing this data
	void* msr_bitmap;               //!< Bitmap to activate MSR I/O VM-exit
	void* io_bitmap_a;              //!< Bitmap to activate IO VM-exit (~ 0x7FFF)
	void* io_bitmap_b;              //!< Bitmap to activate IO VM-exit (~ 0xffff)
	//struct SharedShadowHookData* shared_sh_data;  //!< Shared shadow hook data
};

/// Represents VMM related data associated with each processor
struct ProcessorData {
	SharedProcessorData* shared_data;         //!< Shared data
	void* vmm_stack_limit;                    //!< A head of VA for VMM stack
	struct VmControlStructure* vmxon_region;  //!< VA of a VMXON region
	struct VmControlStructure* vmcs_region;   //!< VA of a VMCS region
	struct EptData* ept_data;                 //!< A pointer to EPT related data
	//struct ShadowHookData* sh_data;           //!< Per-processor shadow hook data
};

/*
* 
* vm_entryctl_requested.fields.ia32e_mode_guest = IsX64();  vm_exitctl_requested.fields.host_address_space_size = IsX64(); ���� __vmx_vmlaunch ִ�е���С����
* ������Щ�쳣 ���Ա� windbg int3�ӹ� Ҳ���Ǳ���
1.cpuid
2.rdmsr
3.wrmsr
//////Ȼ�� windbg �޷��ӹ��쳣  ida�� gdbserver Ҳ�޷��� VmmpHandleVmExit ������ ��ԭ��     /////////
���� ����ʱ����� ��֪   ��  vm_procctl_requested.fields.mov_dr_exiting = true; ���ܱ�����
4. kDrAccess = 29,

���뽻�������Լ������VM-EXIT�¼�(������CPU_BASED_VM_EXEC_CONTROL��EXCEPTION_BITMAP�����ڵ�)��



ԭ�����ӣ�https://blog.csdn.net/zhuhuibeishadiao/article/details/52470491

���봦��
EXIT_REASON_MSR_READ(0x1F)31
EXIT_REASON_MSR_WRITE(0x20)32
EXIT_REASON_CR_ACCESS(0x1C)28
EXIT_REASON_INVD(0xD)13
EXIT_REASON_CPUID(0xA)10
EXIT_REASON_VMCALL(0x12)18
win10 ��Ҫ���




ʵ��1_S:��׼ ���������(vmware),���ĺ� �ҿ����� һ��Сʱ ������
ʵ��1_F:˵�� ʵ��1 ʧ��


ʵ��2_S:

*/