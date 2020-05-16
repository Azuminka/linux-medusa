/* comm.c, (C) 2002 Milan Pikula <www@terminus.sk>
 *
 */
#include <linux/medusa/l3/arch.h>
#include <linux/medusa/l3/registry.h>
#include <linux/medusa/l3/server.h>
#include "l3_internals.h"

inline int is_authserver_reached(medusa_answer_t answer)
{
	return (answer != MED_ERR);
}

inline int is_supported_medusa_answer(medusa_answer_t answer)
{
	return (answer == MED_ALLOW || answer == MED_DENY);
}

medusa_answer_t med_decide(struct medusa_evtype_s *evtype, void *event, void *o1, void *o2)
{
	medusa_answer_t retval;
	struct medusa_authserver_s * authserver;

	if (ARCH_CANNOT_DECIDE(evtype))
		return MED_ALLOW;

	MED_LOCK_W(registry_lock);
#ifdef CONFIG_MEDUSA_PROFILING
	evtype->arg_kclass[0]->l2_to_l4++;
	evtype->arg_kclass[1]->l2_to_l4++;
	evtype->l2_to_l4++;
#endif
	authserver = med_get_authserver();
	if (!authserver) {
		if (evtype->arg_kclass[0]->unmonitor)
			evtype->arg_kclass[0]->unmonitor((struct medusa_kobject_s *) o1);
		if (evtype->arg_kclass[1]->unmonitor)
			evtype->arg_kclass[1]->unmonitor((struct medusa_kobject_s *) o2);
		MED_UNLOCK_W(registry_lock);
		return MED_ALLOW;
	}
	MED_UNLOCK_W(registry_lock);

	((struct medusa_event_s *)event)->evtype_id = evtype;
	retval = authserver->decide(event, o1, o2);
	if (!is_authserver_reached(retval)) {
		/* if L4 returned MED_ERR, it means that authserver could not
		 * respond. We can convert this code into MED_ALLOW, because
		 * the virtual spaces have to intersect otherwise med_decide
		 * would not be called from L2 and therefore we know that
		 * the operation was earlier allowed
		 * */
		retval = MED_ALLOW;
	} else if (!is_supported_medusa_answer(retval)) {
		/* if we received code which is not known or not supported, we
		 * want to DENY the operation since according to protocol, the
		 * authorization server should send Medusa only supported codes
		 * and if it did not, this is suspicious
		 * */
		retval = MED_DENY;
	}
#ifdef CONFIG_MEDUSA_PROFILING
	else {
		MED_LOCK_W(registry_lock);
		evtype->l4_to_l2++;
		MED_UNLOCK_W(registry_lock);
	}
#endif
	med_put_authserver(authserver);
	return retval;
}
