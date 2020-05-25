#include <sel4vmmplatsupport/guest_irq_controller.h>
#include <sel4vmmplatsupport/plat/devices.h>

int vm_install_irq_controller(vm_t *vm)
{
    struct vm_irq_controller_params params = {
        .gic_paddr = GIC_PADDR,
        .gic_dist_paddr = GIC_DIST_PADDR,
        .gic_dist_size = GIC_DIST_SIZE,

#if defined(GIC_VERSION_2)
        .gic_cpu_paddr = GIC_CPU_PADDR,
        .gic_cpu_size = GIC_CPU_SIZE,

        .gic_vcpu_cntr_paddr = GIC_VCPU_CNTR_PADDR,
        .gic_vcpu_cntr_size = GIC_VCPU_CNTR_SIZE,

        .gic_vcpu_paddr = GIC_VCPU_PADDR,
        .gic_vcpu_size = GIC_VCPU_SIZE,

        .version = VM_GIC_V2

#elif defined(GIC_VERSION_3)
        .gic_rdist_paddr = GIC_REDIST_PADDR,
        .gic_rdist_size = GIC_REDIST_SIZE,

        .gic_rdist_sgi_ppi_paddr = GIC_REDIST_SGI_PADDR,
        .gic_rdist_sgi_ppi_size = GIC_REDIST_SGI_SIZE,

        .version = VM_GIC_V3
#else
#error
#endif

    };
    return vm_create_default_irq_controller(vm, &params);
}
