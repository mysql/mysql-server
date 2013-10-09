/*
 *  Copyright (c) 2011, 2012, Oracle and/or its affiliates. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

package com.mysql.clusterj.jdbc.antlr;

import org.antlr.runtime.RecognitionException;
import org.antlr.runtime.BaseRecognizer;

import java.util.List;

/**
 * Generic ErrorListener for RecognitionExceptions.
 * Enables client code to flexibly override error handling/presentation.
 * Author: kroepke
 */
public interface ErrorListener {

    /**
     * Called by reportError to display a recognition error.
     * Normally it is sufficient to implement this method.
     *
     * @param tokenNames
     * @param e
     */
    void displayRecognitionError(String[] tokenNames, RecognitionException e);

    /**
     * The main entry point for reporting recognition errors.
     * Override only if you want to show "spurious" errors during recovery, too.
     * @see org.antlr.runtime.BaseRecognizer.reportError
     * 
     * @param e A runtime exception containing information about the error.
     */
    public void reportError(RecognitionException e);

    /**
     * Generates a header line for error messages, normally containing source file and the position
     * in the file where the error occured.
     *
     * @param e
     * @return String to print as a location information for the error
     */
    public String getErrorHeader(RecognitionException e);

    /**
     * Generates the message string for the actual error occured, must distinguish between
     * the different RecognitionException subclasses.
     *
     * @param e
     * @param tokenNames
     * @return String to print as an explanation of the error
     */
    public String getErrorMessage(RecognitionException e, String[] tokenNames);

    /**
     * Redirects an error message to the appropriate place, relying on the other methods
     * to actually produce a message
     * @param msg Preformatted error message
     */
    public void emitErrorMessage(String msg);

    /**
     * Check whether errors have occurred during recognition. 
     * @return true if errors have occurred
     */
    public boolean hasErrors();

    /**
     * Retrieve the list of errors occurred.
     *
     * @return Might be null for ErrorListeners that don't queue up errors.
     */
    public List<RecognitionException> getErrors();
}
