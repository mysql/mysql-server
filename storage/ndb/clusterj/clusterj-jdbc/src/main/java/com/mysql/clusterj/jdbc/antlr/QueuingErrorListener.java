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
import java.util.ArrayList;

/**
 * Author: kroepke
 */
public class QueuingErrorListener extends BaseErrorListener implements ErrorListener {

    private List<RecognitionException> exceptions;
    private String[] tokenNames;
    private StringBuilder errorBuffer;
    
    public QueuingErrorListener(RecognizerErrorDelegate recognizerErrorDelegate) {
        super(recognizerErrorDelegate);
        exceptions = new ArrayList<RecognitionException>();
    }

    /**
     * Do not display any errors, but queue them up to inspect at a later stage.
     * Useful for writing tests that look for RecognitionExceptions.
     *
     * @param tokenNames
     * @param e
     */
    @Override
    public void displayRecognitionError(String[] tokenNames, RecognitionException e) {
        if (this.tokenNames == null)
            this.tokenNames = tokenNames;
        exceptions.add(e);
    }
    
    @Override
    public void emitErrorMessage(String msg) {
        errorBuffer.append(msg);
        errorBuffer.append("--------------------\n");
    }

    @Override
    public boolean hasErrors() {
        return !exceptions.isEmpty();
    }

    @Override
    public String toString() {
        if (errorBuffer == null) {
            errorBuffer = new StringBuilder();
            for (RecognitionException e : exceptions) {
                super.displayRecognitionError(tokenNames, e);
            }
        }
        return errorBuffer.toString();
    }

    @Override
    public List<RecognitionException> getErrors() {
        return exceptions;
    }
}
