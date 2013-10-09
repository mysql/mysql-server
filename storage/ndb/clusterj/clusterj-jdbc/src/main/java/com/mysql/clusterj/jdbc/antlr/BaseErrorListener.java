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

import java.util.List;

/**
 * Default implementation of an ErrorListener that simply delegates all error reporting back to the
 * Recognizer it came from. 
 * Author: kroepke
 */
public class BaseErrorListener implements ErrorListener {

    protected final RecognizerErrorDelegate recognizerErrorDelegate;
    protected boolean seenErrors = false;

    public BaseErrorListener(final RecognizerErrorDelegate recognizerErrorDelegate) {
        if (recognizerErrorDelegate == null) {
            throw new IllegalArgumentException("You must pass a recognizer delegate");
        }
        this.recognizerErrorDelegate = recognizerErrorDelegate;
    }

    public void displayRecognitionError(final String[] tokenNames, final RecognitionException e) {
        recognizerErrorDelegate.originalDisplayError(tokenNames, e);
        seenErrors = true;
    }

    public void reportError(final RecognitionException e) {
        recognizerErrorDelegate.originalReportError(e);
    }

    public String getErrorHeader(final RecognitionException e) {
        return recognizerErrorDelegate.originalGetErrorHeader(e);
    }

    public String getErrorMessage(final RecognitionException e, final String[] tokenNames) {
        return recognizerErrorDelegate.originalGetErrorMessage(e, tokenNames);
    }

    public void emitErrorMessage(final String msg) {
        recognizerErrorDelegate.originalEmitErrorMessage(msg);
    }

    public boolean hasErrors() {
        return seenErrors;
    }

    public List<RecognitionException> getErrors() {
        return null;
    }
}
