/*
 *  Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
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

import org.antlr.runtime.IntStream;
import org.antlr.runtime.NoViableAltException;
import org.antlr.runtime.Parser;
import org.antlr.runtime.RecognizerSharedState;
import org.antlr.runtime.RecognitionException;
import org.antlr.runtime.Token;
import org.antlr.runtime.TokenStream;

import java.util.List;
import java.util.logging.Level;
import java.util.logging.Logger;

/**
 * Author: kroepke
 */
public abstract class MySQLParser extends Parser implements RecognizerErrorDelegate {

    private static final Logger log = Logger.getLogger(MySQLParser.class.getName());
    
    private ErrorListener errorListener;

    public String getTokenErrorDisplay(Token t) {
        return t.toString();
    }

    public MySQLParser(TokenStream input) {
        super(input);
        errorListener = new BaseErrorListener(this);
    }

    public MySQLParser(TokenStream input, RecognizerSharedState state) {
        super(input, state);
        errorListener = new BaseErrorListener(this);
    }

    public void setErrorListener(ErrorListener listener) {
        this.errorListener = listener;
    }

    public ErrorListener getErrorListener() {
        return errorListener;
    }

    /** Delegate error presentation to our errorListener if that's set, otherwise pass up the chain. */
    public void displayRecognitionError(String[] tokenNames, RecognitionException e) {
        errorListener.displayRecognitionError(tokenNames, e);
    }

    public String getErrorHeader(RecognitionException e) {
        return errorListener.getErrorHeader(e);
    }

    public void emitErrorMessage(String msg) {
        errorListener.emitErrorMessage(msg);
    }

    /* generate more useful error messages for debugging */
    @SuppressWarnings("unchecked")
    public String getErrorMessage(RecognitionException re, String[] tokenNames) {
        if (log.isLoggable(Level.FINEST)) {
            List stack = getRuleInvocationStack(re, this.getClass().getName());
            String msg;
            if (re instanceof NoViableAltException) {
                NoViableAltException nvae = (NoViableAltException)re;
                msg = "  no viable alternative for token="+ re.token + "\n" +
                "  at decision="+nvae.decisionNumber + " state=" + nvae.stateNumber;
                if (nvae.grammarDecisionDescription != null && nvae.grammarDecisionDescription.length() > 0)
                    msg = msg + "\n  decision grammar=<< "+ nvae.grammarDecisionDescription + " >>";
            } else {
                msg = super.getErrorMessage(re, tokenNames);
            }
            return stack + "\n" + msg;
        } else {
            return errorListener.getErrorMessage(re, tokenNames);

        }
   }

    public void reportError(RecognitionException e) {
        errorListener.reportError(e);
    }

    protected Object getCurrentInputSymbol(IntStream input) {
        return super.getCurrentInputSymbol(input);    //To change body of overridden methods use File | Settings | File Templates.
    }

    /* trampoline methods to get to super implementation */
    public void originalDisplayError(String[] tokenNames, RecognitionException e) {
        super.displayRecognitionError(tokenNames, e);
    }

    public void originalReportError(RecognitionException e) {
        super.reportError(e);
    }

    public String originalGetErrorHeader(RecognitionException e) {
        return super.getErrorHeader(e);
    }

    public String originalGetErrorMessage(RecognitionException e, String[] tokenNames) {
        return super.getErrorMessage(e, tokenNames);
    }

    public void originalEmitErrorMessage(String msg) {
        // super.emitErrorMessage(msg);
        log.warning(msg);
    }
}
