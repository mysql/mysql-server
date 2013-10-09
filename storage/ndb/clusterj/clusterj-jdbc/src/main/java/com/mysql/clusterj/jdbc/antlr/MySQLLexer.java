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

import org.antlr.runtime.CharStream;
import org.antlr.runtime.Lexer;
import org.antlr.runtime.NoViableAltException;
import org.antlr.runtime.RecognitionException;
import org.antlr.runtime.RecognizerSharedState;

import java.util.List;
import java.util.logging.Level;
import java.util.logging.Logger;

/**
 * Abstract superclass for MySQL Lexers, for now containing some common code, so it's not in the grammar.
 * Author: kroepke
 */
public abstract class MySQLLexer extends Lexer implements RecognizerErrorDelegate {

    private static final Logger log = Logger.getLogger(MySQLLexer.class.getName());
    
    boolean nextTokenIsID = false;
    private ErrorListener errorListener;


    /**
     * Check ahead in the input stream for a left paren to distinguish between built-in functions
     * and identifiers.
     * TODO: This is the place to support certain SQL modes.
     * @param proposedType the original token type for this input.
     * @return the new token type to emit a token with
     */
    public int checkFunctionAsID(int proposedType) {
        return (input.LA(1) != '(') ? MySQL51Lexer.ID : proposedType;
    }

    public MySQLLexer() {} 

    public MySQLLexer(CharStream input) {
        this(input, new RecognizerSharedState());
        errorListener = new BaseErrorListener(this);
    }

    public MySQLLexer(CharStream input, RecognizerSharedState state) {
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
        //super.emitErrorMessage(msg);
        log.warning(msg);
    }
    
}
