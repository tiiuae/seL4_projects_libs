#include <sel4vmmplatsupport/guest_irq_controller.h>
#include <sel4vmmplatsupport/plat/devices.h>

int vm_install_irq_controller(vm_t *vm)
{
    struct vm_irq_controller_params params = {
        GIC_PADDR,
        GIC_DIST_PADDR,
        GIC_CPU_PADDR,
        GIC_VCPU_CNTR_PADDR,
        GIC_VCPU_PADDR,
        VM_GIC_V2
    };
    return vm_create_default_irq_controller(vm, &params);
}
