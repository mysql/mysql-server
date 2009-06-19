/*
 * Wrapper.java
 */

package jtie;

public class Wrapper {

    // this field may me private, for access from JNI is still possible
    // XXX should this field be final?
    // - it may not under the hashcode() and equals() consistency requirement
    // - it cannot if we want to nullify this field when deleting the
    //   native delegate object through this instance
    // - it should not, for we're likely not loosing performance in exchange
    //   for better, fail-fast robustness
    // - it might for simplicity; however, we can lessen the increase in
    //   entropy by a restricted ("stable") delegate lifecycle:
    //   - attachement happens at construction only, so detachment is a
    //     one-time effect operation too
    private /*final*/ long cdelegate;

    // attaches this wrapper instance to the native delegate
    protected Wrapper(long cdelegate) {
        this.cdelegate = cdelegate;
        System.out.println("<-> jtie.Wrapper(" + Long.toHexString(cdelegate) + ")");
    };

    // detaches this wrapper instance from its native delegate
    protected void detach() {
        System.out.println("<-> jtie.Wrapper.detach()");
        cdelegate = 0L;
    }

    // equals semantics is defined over cdelegate value only
    public boolean equals(Object obj) {
        if (this == obj)
            return true;
        if (!(obj instanceof Wrapper))
            return false;
        final Wrapper wo = (Wrapper)obj;
        return (cdelegate == wo.cdelegate);
    }
    
    // hashcode semantics is defined over cdelegate value only
    public int hashCode() {
        // ok to loose precision, since only a hash
        return (int)cdelegate;
    }

    // string representation is defined over cdelegate value only
    public String toString() {
        return (getClass().getName() + "@" + Long.toHexString(cdelegate));
    }
}
