(define (domain miconic)
(:requirements
    :strips
    :typing
    :negative-preconditions
    :disjunctive-preconditions
    :equality
    :existential-preconditions
    :universal-preconditions
    :conditional-effects
)
(:types
    passenger - object
    floor - object
)
(:constants
    p0 - passenger
    f0 - floor
    f1 - floor
)
(:predicates
    (origin ?x0 - passenger ?x1 - floor)
    (destin ?x0 - passenger ?x1 - floor)
    (above ?x0 - floor ?x1 - floor)
    (boarded ?x0 - passenger)
    (served ?x0 - passenger)
    (lift-at ?x0 - floor)
    (NOT-served ?x0 - passenger)
    (NOT-boarded ?x0 - passenger)
)
(:action up
    :parameters (?f1 - floor ?f2 - floor)
    :precondition (and (lift-at ?f1) (above ?f1 ?f2))
    :effect (and (lift-at ?f2) (not (lift-at ?f1)))
)

(:action down
    :parameters (?f1 - floor ?f2 - floor)
    :precondition (and (lift-at ?f1) (above ?f2 ?f1))
    :effect (and (lift-at ?f2) (not (lift-at ?f1)))
)

(:action stop
    :parameters (?f - floor)
    :precondition (and (lift-at ?f) (boarded p0) (destin p0 ?f) (not (origin p0 ?f)))
    :effect (and (served p0) (not (NOT-served p0)) (not (boarded p0)) (NOT-boarded p0))
)

(:action stop
    :parameters (?f - floor)
    :precondition (and (lift-at ?f) (NOT-boarded p0) (origin p0 ?f) (NOT-served p0))
    :effect (and (boarded p0) (not (NOT-boarded p0)))
)

(:action stop
    :parameters (?f - floor)
    :precondition (and (lift-at ?f) (boarded p0) (destin p0 ?f) (origin p0 ?f) (NOT-served p0))
    :effect (and (served p0) (not (NOT-served p0)) (NOT-boarded p0) (boarded p0))
)

(:action stop
    :parameters (?f - floor)
    :precondition (and (lift-at ?f) (not (destin p0 ?f)) (origin p0 ?f) (NOT-served p0))
    :effect (and (boarded p0) (not (NOT-boarded p0)))
)

(:action stop
    :parameters (?f - floor)
    :precondition (and (lift-at ?f) (boarded p0) (destin p0 ?f) (served p0))
    :effect (and (served p0) (not (NOT-served p0)) (not (boarded p0)) (NOT-boarded p0))
)

)
