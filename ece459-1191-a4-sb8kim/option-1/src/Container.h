#pragma once

#include <cassert>
#include <stdio.h>

template<typename T>
class Container
{
    class Node {
    public:
        T data; 
        Node* next; 

        Node() {
            this->next = nullptr;
        }
    };

    int len = 0;

    Node* head = new Node(); 
    Node* end = new Node();

    void deepCopy(const Container &other) {
        len = other.len;

        // Delete previous linked list, currently commented out for performance, probably need for memory leak prevention though
        Node* cur = head; 
        Node* next; 

        while (cur != nullptr) {
            next = cur->next; 
            delete(cur); 
            cur = next;
        }

        head = new Node(); 
        end = new Node();

        if (other.head) {
            Node* o = other.head;

            head->data = o->data;
            head->next = nullptr; 
            end = head; 
            o = o->next;

            for (int i = 1; i < len; i++) {
                Node* n = new Node(); 
                n->data = o->data; 
                n->next = nullptr; 
                end->next = n; 
                end = n; 
                o = o->next;
            }
        }
    }

public:
    Container() {
        // nop
    }

    ~Container() {
        clear();
    }

    Container(const Container &other) {
        deepCopy(other);    
    }

    Container& operator=(const Container &other) {
        if (this != &other) {
            deepCopy(other);
        }

        return *this;
    }

    int size() const {
        return len;
    }

    T operator[](int idx) {
        if (!(0 <= idx && idx < len)) {
            assert(false && "Accessing index out of range");
        }

        Node* n = head; 
        for (int i = 0; i < idx; i++) {
            n = n->next; 
        }
        return n->data; 
    }

    void clear() {
        len = 0;

        Node* cur = head; 
        Node* next; 

        while (cur != nullptr) {
            next = cur->next; 
            delete(cur); 
            cur = next;
        }

        head = new Node(); 
        end = new Node();
    }

    T front() const {
        return head->data;
    }

    void pushFront(T item) {
        len++;
        if (len == 1) {
            head->data = item; 
            head->next = nullptr;
            end = head;  
        }
        else {
            Node* n = new Node(); 
            n->data = item; 
            n->next = head; 
            head = n;             
        }
    }

    void pushBack(T item) {
        len++;
        if (len == 1) {
            head->data = item; 
            head->next = nullptr; 
            end = head; 
        }
        else {
            Node* n = new Node(); 
            n->data = item; 
            n->next = nullptr; 
            end->next = n; 
            end = n;
        }
    }

    T popFront() {
        if (len < 1) {
            assert(false && "Trying to pop an empty Container");
        }

        T front = head->data; 
        len--;
        if (len == 0) {
            head->next = nullptr; 
            end = head; 
        }
        else {
            Node* n = head; 
            head = n->next; 
            delete(n); 
        } 
        return front; 
    }

    void push(T item) {
        pushBack(item);
    }

    T pop() {
        return popFront();
    }
};