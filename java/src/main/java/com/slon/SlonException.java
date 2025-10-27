package com.slon;

/**
 * Runtime exception thrown when parsing SLON fails.
 */
public class SlonException extends RuntimeException {
    public SlonException(String message) {
        super(message);
    }

    public SlonException(String message, Throwable cause) {
        super(message, cause);
    }
}
