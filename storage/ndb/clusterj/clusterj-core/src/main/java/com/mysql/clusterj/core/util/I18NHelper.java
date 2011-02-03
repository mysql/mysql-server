/*
   Copyright 2010 Sun Microsystems, Inc.
   All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package com.mysql.clusterj.core.util;

import java.util.*;
import java.text.MessageFormat;
import java.security.AccessController;
import java.security.PrivilegedAction;

import com.mysql.clusterj.ClusterJFatalInternalException;

/** Helper class for constructing messages from bundles.  The intended usage
 * of this class is to construct a new instance bound to a bundle, as in
 * <P>
 * <code>I18NHelper local = 
 *  I18NHelper.getInstance("com.mysql.clusterj.core.Bundle");</code>
 * <P>
 * This call uses the class loader that loaded the I18NHelper class to find
 * the specified Bundle. The class provides two overloaded getInstance
 * methods allowing to specify a different class loader: 
 * {@link #getInstance(Class cls)} looks for a bundle
 * called "Bundle.properties" located in the package of the specified class 
 * object and {@link #getInstance(String bundleName,ClassLoader loader)} 
 * uses the specified class loader to find the bundle.
 * <P>
 * Subsequently, instance methods can be used to format message strings 
 * using the text from the bundle, as in 
 * <P>
 * <code>throw new JDOFatalInternalException (local.message("ERR_NoMetadata", 
 * cls.getName()));</code>
 */        
public class I18NHelper {

    /** The logger */
    private static Logger logger = LoggerFactoryService.getFactory()
            .getInstance(I18NHelper.class);

    /** Bundles that have already been loaded 
     */
    private static Hashtable<String, ResourceBundle> bundles = new Hashtable<String, ResourceBundle>();
    
    /** Helper instances that have already been created 
     */
    private static Hashtable<String, I18NHelper> helpers = new Hashtable<String, I18NHelper>();
    
    /** The default locale for this VM.
     */
    private static Locale       locale = Locale.getDefault();

    /** The name of the bundle used by this instance of the helper.
     */
    private final String        bundleName;

    /** The bundle used by this instance of the helper.
     */
    private ResourceBundle      bundle = null;

    /** Throwable if ResourceBundle couldn't be loaded
     */
    private Throwable           failure = null;

    /** The unqualified standard name of a bundle. */
    private static final String bundleSuffix = ".Bundle";    // NOI18N

    /** Constructor */
    private I18NHelper() {
        this.bundleName = null;
    }

    /** Constructor for an instance bound to a bundle.
     * @param bundleName the name of the resource bundle
     * @param loader the class loader from which to load the resource
     * bundle
     */
    private I18NHelper (String bundleName, ClassLoader loader) {
        this.bundleName = bundleName;
        try {
            bundle = loadBundle (bundleName, bundleName, loader);
        }
        catch (Throwable e) {
            failure = e;
        }
    }
    
    /** An instance bound to a bundle. This method uses the current class 
     * loader to find the bundle.
     * @param bundleName the name of the bundle
     * @return the helper instance bound to the bundle
     */
    public static I18NHelper getInstance (String bundleName) {
        return getInstance (bundleName, I18NHelper.class.getClassLoader());
    }

    /** An instance bound to a bundle. This method figures out the bundle name
     * for the class object's package and uses the class' class loader to
     * find the bundle. Note, the specified class object must not be
     * <code>null</code>.
     * @param cls the class object from which to load the resource bundle
     * @return the helper instance bound to the bundle
     */
    public static I18NHelper getInstance (final Class<?> cls) {
        ClassLoader classLoader = AccessController.doPrivileged (
            new PrivilegedAction<ClassLoader> () {
                public ClassLoader run () {
                    return cls.getClassLoader();
                }
            }
            );
        String bundle = getPackageName (cls.getName()) + bundleSuffix;
        return getInstance (bundle, classLoader);
    }

    /** An instance bound to a bundle. This method uses the specified class
     * loader to find the bundle. Note, the specified class loader must not
     * be <code>null</code>.
     * @param bundleName the name of the bundle
     * @param loader the class loader from which to load the resource
     * bundle
     * @return the helper instance bound to the bundle
     */
    public static I18NHelper getInstance (String bundleName, 
                                          ClassLoader loader) {
        I18NHelper helper = helpers.get (bundleName);
        if (helper != null) {
            return helper;
        }
        helper = new I18NHelper(bundleName, loader);
        helpers.put (bundleName, helper);
        // if two threads simultaneously create the same helper, return the first
        // one to be put into the Hashtable.  The other will be garbage collected.
        return helpers.get (bundleName);
    }

    /** Message formatter
     * @param messageKey the message key
     * @return the resolved message text
     */
    public String message (String messageKey) {
        assertBundle (messageKey);
        return getMessage (bundle, messageKey);
    }

    /** Message formatter
     * @param messageKey the message key
     * @param arg1 the first argument
     * @return the resolved message text
     */
    public String message (String messageKey, Object arg1) {
        assertBundle (messageKey);
        return getMessage (bundle, messageKey, arg1);
    }

    /** Message formatter
     * @param messageKey the message key
     * @param arg1 the first argument
     * @param arg2 the second argument
     * @return the resolved message text
     */
    public String message (String messageKey, Object arg1, Object arg2) {
        assertBundle (messageKey);
        return getMessage (bundle, messageKey, arg1, arg2);
    }

    /** Message formatter
     * @param messageKey the message key
     * @param args the array of arguments
     * @return the resolved message text
     */
    public String message (String messageKey, Object... args) {
        assertBundle (messageKey);
        return getMessage (bundle, messageKey, args);
    }

    /** Message formatter
     * @param messageKey the message key
     * @param arg the argument
     * @return the resolved message text
     */
    public String message (String messageKey, int arg) {
        assertBundle (messageKey);
        return getMessage(bundle, messageKey, arg);
    }
    
    /** Message formatter
     * @param messageKey the message key
     * @param arg the argument
     * @return the resolved message text
     */
    public String message (String messageKey, boolean arg) {
        assertBundle (messageKey);
        return getMessage(bundle, messageKey, arg);
    }
    
    /** Returns the resource bundle used by this I18NHelper.
     * @return the associated resource bundle
     */
    public ResourceBundle getResourceBundle () {
        assertBundle ();
        return bundle;
    }
    
    //========= Internal helper methods ==========

    /**
     * Load ResourceBundle by bundle name
     * @param bundleName the name of the bundle
     * @param loader the class loader from which to load the resource bundle
     * @return  the ResourceBundle
     */
    final private static ResourceBundle loadBundle(
                String original, String bundleName, ClassLoader loader) {
        ResourceBundle messages = bundles.get(bundleName);

        if (messages == null) //not found as loaded - add
        {
            try {
                if (loader != null) {
                    messages = ResourceBundle.getBundle(bundleName, locale, loader);
                } else {
                    // the library was loaded by the boostrap class loader
                    messages = ResourceBundle.getBundle(bundleName, locale,
                            getSystemClassLoaderPrivileged());
                }
                bundles.put(bundleName, messages);
            } catch (java.util.MissingResourceException ex) {
                // recursively try to find the Bundle in the next higher package
                String superBundleName = removeDirectoryName(bundleName);
                if (superBundleName == null) {
                    throw new ClusterJFatalInternalException(
                            "Missing resource bundle " + original);
                }
                messages = loadBundle(original, superBundleName, loader);
            }
        }
        return messages;
    }

    /** Assert resources available
     * @throws JDOFatalInternalException if the resource bundle could not
     * be loaded during construction.
     */
    private void assertBundle () {
        if (failure != null)
            throw new ClusterJFatalInternalException (
                "No resources could be found for bundle:\"" + 
                bundleName + "\" ", failure);
    }
    
    /** Assert resources available
     * @param key the message key 
     * @throws JDOFatalInternalException if the resource bundle could not
     * be loaded during construction.
     */
    private void assertBundle (String key) {
        if (failure != null)
            throw new ClusterJFatalInternalException (
                "No resources could be found for bundle: " + bundleName
                + " to annotate error message key:\""
                + key + "\"", failure);
    }

    /**
     * Returns message as <code>String</code>
     * @param messages the resource bundle
     * @param messageKey the message key
     * @return the resolved message text
     */
    final private static String getMessage(ResourceBundle messages, String messageKey) 
    {
        return messages.getString(messageKey);
    }

    /**
     * Formats message by adding array of arguments
     * @param messages the resource bundle
     * @param messageKey the message key
     * @param msgArgs an array of arguments to substitute into the message
     * @return the resolved message text
     */
    final private static String getMessage(ResourceBundle messages, 
            String messageKey, Object[] msgArgs) 
    {
        for (int i=0; i<msgArgs.length; i++) {
            if (msgArgs[i] == null) msgArgs[i] = ""; // NOI18N
        }
        MessageFormat formatter = new MessageFormat(messages.getString(messageKey));
        return formatter.format(msgArgs);
    }
    
    /**
     * Formats message by adding an <code>Object</code> argument.
     * @param messages the resource bundle
     * @param messageKey the message key
     * @param arg the argument
     * @return the resolved message text
     */
    final private static String getMessage(ResourceBundle messages, 
            String messageKey, Object arg) 
    {
        Object []args = {arg};
        return getMessage(messages, messageKey, args);
    }
    
    /**
     * Formats message by adding two <code>Object</code> arguments.
     * @param messages the resource bundle
     * @param messageKey the message key
     * @param arg1 the first argument
     * @param arg2 the second argument
     * @return the resolved message text
     */
    final private static String getMessage(ResourceBundle messages, 
            String messageKey, Object arg1, Object arg2) 
    {
        Object []args = {arg1, arg2};
        return getMessage(messages, messageKey, args);
    }
    
    /**
     * Formats message by adding an <code>int</code> as an argument.
     * @param messages the resource bundle
     * @param messageKey the message key
     * @param arg the argument
     * @return the resolved message text
     */
    final private static String getMessage(ResourceBundle messages, 
            String messageKey, int arg) 
    {
        Object []args = {new Integer(arg)};
        return getMessage(messages, messageKey, args);
    }
    
    /**
     * Formats message by adding a <code>boolean</code> as an argument.
     * @param messages the resource bundle
     * @param messageKey the message key
     * @param arg the argument
     * @return the resolved message text
     */
    final private static String getMessage(ResourceBundle messages, 
            String messageKey, boolean arg) 
    {
        Object []args = {String.valueOf(arg)};
        return getMessage(messages, messageKey, args);
    }

    /**  
     * Returns the package portion of the specified class.
     * @param className the name of the class from which to extract the 
     * package 
     * @return package portion of the specified class
     */   
    final private static String getPackageName(final String className)
    { 
        final int index = className.lastIndexOf('.');
        return ((index != -1) ? className.substring(0, index) : ""); // NOI18N
    }

    /** Return the bundle name of the super package. For example,
     * if the bundleName is com.mysql.cluster.util.deeper.Bundle,
     * return com.mysql.cluster.util.Bundle.
     * @param bundleName the bundle name
     * @return the bundle name of the super package
     */
    private static String removeDirectoryName(String bundleName) {
        String result;
        int lastDot = bundleName.lastIndexOf(".");
        String packageName = bundleName.substring(0, lastDot);
        String suffix = bundleName.substring(lastDot);
        int index = packageName.lastIndexOf(".");
        if (index == -1) {
            return null;
        }
        String superPackageName = packageName.substring(0, index);
        result = superPackageName + suffix;

        if (logger.isDebugEnabled()) {
            logger.debug("bundleName is: " + bundleName + 
                    "; superPackageName is: " + superPackageName + 
                    "; suffix is: " + suffix + 
                    "; packageName is: " + packageName + 
                    "; returning: " + result);
        }
        return result;
    }

    /**
     * Get the system class loader. This must be done in a doPrivileged 
     * block because of security.
     */
    private static ClassLoader getSystemClassLoaderPrivileged() {
        return AccessController.doPrivileged (
            new PrivilegedAction<ClassLoader> () {
                public ClassLoader run () {
                    return ClassLoader.getSystemClassLoader();
                }
            }
        );
    }
}
